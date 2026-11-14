static unsigned failures;
#define CHECK(c, why) do { if (!(c)) { fprintf(stderr, "FAIL %s t=%u left=%d master=%d\n", why, (unsigned)(now - start), left, master); ++failures; } } while (0)
static void set_mode(unsigned q, uint8_t v) {
    qmk_settings_proto_t p = {q}; assert(sm62_setting_set(&p, &v, 1) == 0);
}
static uint8_t get(unsigned q) {
    qmk_settings_proto_t p = {q}; uint8_t v = 0xFF;
    assert(sm62_setting_get(&p, &v, 1) == 0); return v;
}
int main(int argc, char **argv) {
    assert(argc == 5);
    left = atoi(argv[2]); master = atoi(argv[3]);
    uint32_t start = (uint32_t)strtoul(argv[4], NULL, 0);
    now = start; memset(eeprom, 0xFF, sizeof(eeprom));
    kb_settings_extra_init(); sm62_settings_register_rpc();
    unsigned initial_writes = writes;
    unsigned local_q = left ? 230 : 235, remote_q = left ? 235 : 230;
    uint8_t remote_before = get(remote_q);
    /* Always a changed value on either physical half. */
    set_mode(local_q, 1);
    CHECK(sm62_local_mode_transition_pending(), "changed local SET starts pending");
    CHECK(get(231) == 0 && get(236) == (left ? 1 : 0), "ordinary GET retains production ship normalization");
    CHECK(get(remote_q) == remote_before, "peer GET value preserved");
    if (!strcmp(argv[1], "repeated")) {
        for (unsigned elapsed = 100; elapsed <= 4500; elapsed += 100) {
            now = start + elapsed;
            CHECK(get(230) == (left ? 1 : 0), "QSID230 value");
            CHECK(get(235) == (left ? 2 : 1), "QSID235 value");
            bool pending = sm62_local_mode_transition_pending();
            if (elapsed < 4000) CHECK(pending, "100ms GETs retain response gap before bound");
            else CHECK(!pending, "100ms GETs cannot extend absolute 4000ms bound");
            sm62_settings_sync_task();
        }
    } else if (!strcmp(argv[1], "single")) {
        now = start + 100; CHECK(get(local_q) == 1, "single verified GET value");
        now = start + 349; CHECK(sm62_local_mode_transition_pending(), "single GET 249ms gap");
        now = start + 350; CHECK(!sm62_local_mode_transition_pending(), "single GET 250ms release");
        now = start + 1000; sm62_settings_sync_task();
    } else if (!strcmp(argv[1], "late")) {
        now = start + 3900; CHECK(get(local_q) == 1, "late GET value");
        now = start + 3999; CHECK(sm62_local_mode_transition_pending(), "late GET before absolute bound");
        now = start + 4000; CHECK(!sm62_local_mode_transition_pending(), "late GET cannot extend absolute bound");
        sm62_settings_sync_task();
    } else if (!strcmp(argv[1], "remote")) {
        now = start + 4000; CHECK(!sm62_local_mode_transition_pending(), "settle initial local change");
        set_mode(remote_q, 3);
        CHECK(!sm62_local_mode_transition_pending(), "remote SET does not gate physical local view");
        CHECK(get(remote_q) == 3 && get(local_q) == 1, "physical sides independent of master role");
        now = start + 8000; sm62_settings_sync_task();
    } else {
        assert(!strcmp(argv[1], "none"));
        now = start + 3999; CHECK(sm62_local_mode_transition_pending(), "no GET bounded wait");
        now = start + 4000; CHECK(!sm62_local_mode_transition_pending(), "no GET release at 4000ms");
        sm62_settings_sync_task();
    }
    if (strcmp(argv[1], "single")) CHECK(writes > initial_writes, "deferred EEPROM persistence still runs");
    CHECK(master ? sends > 0 : sends == 0, "split transmission follows master, not physical side");
    printf("%s %s left=%d master=%d epoch=%u failures=%u writes=%u sends=%u\n", failures ? "FAIL" : "PASS", argv[1], left, master, start, failures, writes, sends);
    return failures ? 1 : 0;
}
