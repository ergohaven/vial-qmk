#! /bin/sh

./util/docker_cmd.sh make ergohaven/hpd:ergohaven
./util/docker_cmd.sh make ergohaven/k02:ergohaven
./util/docker_cmd.sh make ergohaven/k03:enc-left
./util/docker_cmd.sh make ergohaven/k03:enc-left-right
./util/docker_cmd.sh make ergohaven/k03:enc-right
./util/docker_cmd.sh make ergohaven/k03:ergohaven
./util/docker_cmd.sh make ergohaven/macropad:ergohaven
./util/docker_cmd.sh make ergohaven/planeta/rev1:v1
./util/docker_cmd.sh make ergohaven/planeta/rev2:v2
./util/docker_cmd.sh make ergohaven/planeta/rev2:hid
./util/docker_cmd.sh make ergohaven/remnant:ergohaven
./util/docker_cmd.sh make ergohaven/velvet/rev1:v1
./util/docker_cmd.sh make ergohaven/velvet/rev2:v2