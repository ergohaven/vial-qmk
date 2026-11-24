#!/usr/bin/env python3
"""Publish committed main through ci/source-check, without a PR or a build.

Run with the authorized GH_TOKEN and repository Git identity configured:
    python3 util/publish_ergohaven.py

The same command resumes an interrupted publication without repushing the CI
ref. Production moves only after GitHub recognizes its exact required checks.
Protection settings, tags and releases are never changed.
"""

import argparse
import datetime
import fcntl
import json
import os
from pathlib import Path
import subprocess
import sys
import time
from urllib.parse import quote

ROOT = Path(__file__).resolve().parents[1]
REPO = "ergohaven/vial-qmk"
REMOTE = "https://github.com/" + REPO + ".git"
PRODUCTION = "main"
STAGING = "ci/source-check"
ACTOR = "kissetfall"
EMAIL = "j@ergohaven.xyz"
APP_ID = 15368
CHECK_NAMES = {"Check Vial UIDs", "Ergohaven required checks"}


class PublishError(RuntimeError):
    pass


def require(condition, message):
    if not condition:
        raise PublishError(message)


def protection_checks(protection, rules):
    """Fail closed when the live destination no longer permits this route."""
    require(not rules, "Additional branch rules require a publication-policy review")
    require(not protection.get("required_pull_request_reviews"), "Production requires a PR")
    require(not protection.get("restrictions"), "Production has restricted push actors")
    for key in ("lock_branch", "required_signatures", "allow_force_pushes", "allow_deletions"):
        require(not protection.get(key, {}).get("enabled"), "Unsupported protection setting: " + key)
    status = protection.get("required_status_checks") or {}
    checks = status.get("checks") or []
    require(status.get("strict") is True, "Strict required checks must remain enabled")
    require(CHECK_NAMES <= {c.get("context") for c in checks}, "Mandatory product checks are missing")
    require(set(status.get("contexts", [])) == {c.get("context") for c in checks},
            "Required check names and App bindings disagree")
    require(all(isinstance(c.get("app_id"), int) and c["app_id"] > 0 for c in checks),
            "Every required check must have an explicit GitHub App binding")
    require(all(c["app_id"] == APP_ID for c in checks if c["context"] in CHECK_NAMES),
            "Product checks must belong to GitHub Actions")
    return {(c["context"], c["app_id"]) for c in checks}


def rollup_passes(rollup, required):
    """REST run success never substitutes for GitHub's required-check rollup."""
    if not required or not rollup or rollup.get("state") != "SUCCESS":
        return False
    contexts = rollup.get("contexts") or {}
    nodes = contexts.get("nodes") or []
    if contexts.get("pageInfo", {}).get("hasNextPage") or contexts.get("totalCount") != len(nodes):
        return False
    passed = set()
    for node in nodes:
        app = (node.get("checkSuite") or {}).get("app") or {}
        if (node.get("__typename") == "CheckRun" and node.get("status") == "COMPLETED"
                and node.get("conclusion") == "SUCCESS"):
            passed.add((node.get("name"), app.get("databaseId")))
    return required <= passed


class Publisher:
    def __init__(self, receipt):
        self.receipt = receipt

    def command(self, args, *, optional=False):
        result = subprocess.run(args, cwd=ROOT, capture_output=True, text=True, timeout=180,
                                env={**os.environ, "GIT_TERMINAL_PROMPT": "0"})
        if result.returncode and not optional:
            raise PublishError("Command failed: " + " ".join(args[:3]) + "\n" + result.stderr)
        return result

    def git(self, *args):
        return self.command(["git", *args]).stdout.strip()

    def api(self, path, *args):
        result = self.command(["gh", "api", "--hostname", "github.com", path, *args])
        payload = json.loads(result.stdout)
        require(not isinstance(payload, dict) or not payload.get("errors"), "GitHub API returned errors")
        return payload

    def save(self, name, value):
        text = json.dumps(value, indent=2) + "\n"
        token = os.environ.get("GH_TOKEN", "")
        if token:
            text = text.replace(token, "[REDACTED]")
        (self.receipt / name).write_text(text)

    def identity(self):
        require(bool(os.environ.get("GH_TOKEN")), "Load the authorized credential as GH_TOKEN first")
        require(self.api("user")["login"] == ACTOR, "Unexpected GitHub writer")
        require(self.git("config", "user.name") == ACTOR, "Unexpected repository Git author")
        require(self.git("config", "user.email") == EMAIL, "Unexpected repository Git email")
        for kind in ((), ("--push",)):
            require(self.git("remote", "get-url", *kind, "origin") == REMOTE, "Unexpected origin URL")

    def branch(self, name):
        refs = self.api("repos/" + REPO + "/git/matching-refs/heads/" + name)
        matches = [r for r in refs if r["ref"] == "refs/heads/" + name]
        require(len(matches) <= 1, "Ambiguous branch response")
        return matches[0]["object"]["sha"] if matches else None

    def policy(self):
        path = "repos/" + REPO + "/branches/" + PRODUCTION
        branch = self.api(path)
        require(branch["protected"] is True, "Production protection must remain enabled")
        protection = self.api(path + "/protection")
        rules = self.api("repos/" + REPO + "/rules/branches/" + PRODUCTION)
        return branch["commit"]["sha"], protection_checks(protection, rules), protection

    def ancestry(self, base, candidate):
        result = self.command(["git", "merge-base", "--is-ancestor", base, candidate], optional=True)
        require(result.returncode == 0, "Candidate must descend from the live destination; fetch and reconcile first")

    def verify_remote_commit(self, sha, tree):
        commit = self.api("repos/" + REPO + "/git/commits/" + sha)
        require(commit["sha"] == sha and commit["tree"]["sha"] == tree, "Remote source tree mismatch")
        for role in ("author", "committer"):
            require(commit[role]["name"] == ACTOR and commit[role]["email"] == EMAIL,
                    "Remote publication commit has unexpected attribution")

    def push(self, candidate, ref):
        self.identity()
        result = self.command(["git", "-c", "credential.helper=", "-c",
                               "credential.helper=!gh auth git-credential", "-c", "push.followTags=false",
                               "push", "origin", candidate + ":refs/heads/" + ref], optional=True)
        name = "push-" + ref.replace("/", "-") + ".json"
        self.save(name, {"candidate": candidate, "ref": ref, "returncode": result.returncode,
                         "stdout": result.stdout, "stderr": result.stderr})
        # Read back even after a transport failure: the server might have accepted it.
        actual = self.branch(ref)
        require(actual == candidate, "Destination readback mismatch; inspect the saved push result")
        require("bypassed" not in (result.stdout + result.stderr).lower(),
                "GitHub reported a protection bypass; destination changed, stop further writes")
        require(result.returncode == 0, "Push outcome needs reconciliation; do not repeat it blindly")

    def rollup(self, candidate):
        query = """query($sha:String!) { repository(owner:"ergohaven", name:"vial-qmk") {
          object(expression:$sha) { ... on Commit { oid statusCheckRollup {
            state contexts(first:100) { totalCount pageInfo { hasNextPage }
              nodes { __typename ... on CheckRun { name status conclusion
                checkSuite { app { databaseId slug } } } }
            }
          } } }
        } }"""
        payload = self.api("graphql", "-f", "query=" + query, "-f", "sha=" + candidate)
        commit = payload["data"]["repository"]["object"]
        require(commit and commit["oid"] == candidate, "GitHub commit identity mismatch")
        return commit.get("statusCheckRollup")

    def ci_run(self, candidate):
        path = ("repos/" + REPO + "/actions/runs?head_sha=" + candidate
                + "&event=push&branch=" + quote(STAGING, safe="") + "&per_page=100")
        payload = self.api(path)
        require(payload["total_count"] <= len(payload["workflow_runs"]), "Incomplete Actions run inventory")
        runs = [r for r in payload["workflow_runs"] if r["path"] == ".github/workflows/ci.yml"
                and r["head_sha"] == candidate and r["head_branch"] == STAGING and r["event"] == "push"]
        require(len(runs) <= 1, "Multiple CI waves require explicit reconciliation")
        return runs[0] if runs else None

    def publish(self, timeout):
        self.identity()
        require(self.git("branch", "--show-current") == "main", "Publish only from canonical main")
        require(not self.git("status", "--porcelain=v1", "--untracked-files=all"),
                "Commit only the accepted source inventory before publishing")
        candidate = self.git("rev-parse", "HEAD")
        tree = self.git("rev-parse", "HEAD^{tree}")
        base, required, protection = self.policy()
        self.save("intent.json", {"candidate": candidate, "tree": tree, "base": base,
                                   "staging": STAGING, "production": PRODUCTION, "protection": protection})
        if base == candidate:
            self.verify_remote_commit(candidate, tree)
            return candidate
        self.ancestry(base, candidate)
        incoming = self.git("log", "--format=%H%x09%an%x09%ae%x09%cn%x09%ce", base + ".." + candidate)
        for line in incoming.splitlines():
            require(line.split("\t")[1:] == [ACTOR, EMAIL, ACTOR, EMAIL],
                    "Incoming unpublished history has an unexpected author or committer")
        require(incoming, "No candidate commits")
        source_rules = self.api("repos/" + REPO + "/rules/branches/" + quote(STAGING, safe=""))
        require(not source_rules, "CI ref has unsupported branch rules")
        source = self.branch(STAGING)
        if source:
            branch = self.api("repos/" + REPO + "/branches/" + quote(STAGING, safe=""))
            require(not branch["protected"], "CI ref is protected; cannot stage unchecked source")
            self.ancestry(source, candidate)
        if source != candidate:
            fresh_base, fresh_required, fresh_protection = self.policy()
            require((fresh_base, fresh_required, fresh_protection) == (base, required, protection),
                    "Production moved or its protection changed before CI staging")
            self.push(candidate, STAGING)
        self.verify_remote_commit(candidate, tree)
        print("CI source published: " + candidate + " on " + STAGING, flush=True)
        deadline = time.monotonic() + timeout
        while True:
            require(self.branch(STAGING) == candidate, "CI ref moved; publication stopped")
            run = self.ci_run(candidate)
            rollup = self.rollup(candidate)
            self.save("checks.json", {"candidate": candidate, "run": run, "rollup": rollup})
            if run and run["status"] == "completed":
                require(run["conclusion"] == "success", "Lightweight CI failed: " + run["html_url"])
                if rollup_passes(rollup, required):
                    break
                require(not rollup or rollup.get("state") not in {"FAILURE", "ERROR"},
                        "GitHub's required-check rollup failed; production unchanged")
            require(time.monotonic() < deadline,
                    "Required-check deadline reached; source remains staged, production unchanged")
            time.sleep(5)
        self.identity()
        fresh_base, fresh_required, fresh_protection = self.policy()
        require((fresh_base, fresh_required, fresh_protection) == (base, required, protection),
                "Production moved or protection changed while CI ran")
        require(self.branch(STAGING) == candidate and self.git("rev-parse", "HEAD") == candidate,
                "Candidate changed before promotion")
        require(not self.git("status", "--porcelain=v1", "--untracked-files=all"), "Worktree changed during CI")
        require(rollup_passes(self.rollup(candidate), required), "Required checks no longer pass")
        self.push(candidate, PRODUCTION)
        self.verify_remote_commit(candidate, tree)
        after, after_required, after_protection = self.policy()
        require(after == candidate and after_required == required and after_protection == protection,
                "Production or its protection differs after promotion")
        self.save("published.json", {"sha": candidate, "tree": tree, "base": base,
                                     "staging_run_url": run["html_url"], "protection_unchanged": True})
        return candidate


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--timeout", type=int, default=480, help="Maximum CI wait in seconds")
    args = parser.parse_args()
    require(args.timeout > 0, "Timeout must be positive")
    git_dir = Path(subprocess.check_output(["git", "rev-parse", "--absolute-git-dir"], cwd=ROOT, text=True).strip())
    with (git_dir / "ergohaven-publish.lock").open("a") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        receipt = git_dir / "ergohaven-publish" / datetime.datetime.now().strftime("%Y%m%d-%H%M%S-%f")
        receipt.mkdir(parents=True)
        publisher = Publisher(receipt)
        print("Publication receipt: " + str(receipt), flush=True)
        try:
            sha = publisher.publish(args.timeout)
        except (PublishError, OSError, ValueError, KeyError, subprocess.SubprocessError) as error:
            message = str(error).replace(os.environ.get("GH_TOKEN") or "\0", "[REDACTED]")
            publisher.save("stopped.json", {"error": message})
            print("Publication stopped: " + message, file=sys.stderr)
            return 1
        print("Published: https://github.com/" + REPO + "/tree/" + sha, flush=True)
        return 0


if __name__ == "__main__":
    sys.exit(main())
