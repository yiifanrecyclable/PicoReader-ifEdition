/* SPDX-License-Identifier: Apache-2.0
 * 中文：十五分钟真实采样，最多七天；TF双槽写入与回读，损坏的新槽不会替换旧记录。
 * English: Measured fifteen-minute samples, at most seven days; verify alternating TF slots so a bad write cannot replace the last valid history.
 */
#include "battery_history.h"
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "esp_heap_caps.h"
#ifndef BATTERY_HISTORY_DIR
#define BATTERY_HISTORY_DIR "/sdcard/.if-ink"
#endif
typedef struct {
    char magic[8];
    uint32_t generation, count, checksum;
    battery_sample_t samples[BATTERY_HISTORY_MAX];
} history_file_t;
static history_file_t *s_history;
static bool s_media;
static int s_slot = -1;
static uint32_t checksum(const history_file_t *f) {
    uint32_t h = 2166136261U;
    const unsigned char *bytes = (const unsigned char *)f;
    for (size_t i = 0; i < sizeof(*f); ++i) {
        unsigned char b = i >= offsetof(history_file_t, checksum) &&
                          i < offsetof(history_file_t, checksum) + sizeof(f->checksum) ? 0 : bytes[i];
        h = (h ^ b) * 16777619U;
    }
    return h;
}
static bool valid(const history_file_t *f) {
    if (memcmp(f->magic, "IFBAT001", 8) || f->count > BATTERY_HISTORY_MAX || f->checksum != checksum(f)) return false;
    for (uint32_t i = 0; i < f->count; ++i) {
        const battery_sample_t *s = &f->samples[i];
        if (s->at < 1704067200U || s->percent > 100 || s->charging > 1 || s->millivolts > 6000 ||
            (i && s->at <= f->samples[i-1].at)) return false;
    }
    return true;
}
static bool read_slot(int slot, history_file_t *out) {
    char path[96]; snprintf(path, sizeof(path), BATTERY_HISTORY_DIR "/battery-%d.bin", slot);
    FILE *f = fopen(path, "rb"); if (!f) return false;
    bool ok = fread(out, 1, sizeof(*out), f) == sizeof(*out) && fgetc(f) == EOF;
    if (fclose(f) != 0) ok = false;
    return ok && valid(out);
}
static bool allocate(void) {
    if (!s_history) s_history = heap_caps_calloc(1, sizeof(*s_history), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!s_history) return false;
    if (memcmp(s_history->magic, "IFBAT001", 8)) memcpy(s_history->magic, "IFBAT001", 8);
    return true;
}
void battery_history_reset(void) {
    free(s_history); s_history = NULL; s_media = false; s_slot = -1;
}
static bool load(void) {
    history_file_t *temp = heap_caps_calloc(1, sizeof(*temp), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!temp) return false;
    // 换卡不混入前一张卡的历史；无卡的会话采样不会写到新卡。
    // Do not merge another card's history or persist cardless session samples onto a new card.
    memset(s_history, 0, sizeof(*s_history)); memcpy(s_history->magic, "IFBAT001", 8); s_slot = -1;
    for (int slot = 0; slot < 2; ++slot) if (read_slot(slot, temp) &&
            (s_slot < 0 || (int32_t)(temp->generation - s_history->generation) > 0)) {
        memcpy(s_history, temp, sizeof(*temp)); s_slot = slot;
    }
    free(temp);
    return true;
}
static void save(void) {
    if (!s_media || (mkdir(BATTERY_HISTORY_DIR, 0755) != 0 && errno != EEXIST)) return;
    int next = s_slot == 0 ? 1 : 0;
    uint32_t previous = s_history->generation;
    s_history->generation++; s_history->checksum = checksum(s_history);
    char path[96]; snprintf(path, sizeof(path), BATTERY_HISTORY_DIR "/battery-%d.bin", next);
    FILE *f = fopen(path, "wb"); if (!f) { s_history->generation = previous; return; }
    bool ok = fwrite(s_history, 1, sizeof(*s_history), f) == sizeof(*s_history);
    if (fflush(f) != 0) ok = false;
    if (fclose(f) != 0) ok = false;
    history_file_t *temp = heap_caps_calloc(1, sizeof(*temp), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!temp) ok = false;
    if (ok) ok = read_slot(next, temp) && !memcmp(temp, s_history, sizeof(*temp));
    free(temp);
    if (ok) s_slot = next;
    else s_history->generation = previous;
}
void battery_history_observe(uint32_t at, int percent, unsigned millivolts, bool charging, bool media_ready) {
    if (at < 1704067200U || percent < 0 || percent > 100 || millivolts > 6000 || !allocate()) return;
    if (media_ready != s_media) {
        if (media_ready && !load()) return;
        s_media = media_ready;
        if (!media_ready) { memset(s_history, 0, sizeof(*s_history)); memcpy(s_history->magic, "IFBAT001", 8); s_slot = -1; }
    }
    size_t n = s_history->count;
    if (n && (at <= s_history->samples[n-1].at || at-s_history->samples[n-1].at < BATTERY_HISTORY_INTERVAL)) return;
    size_t drop = 0;
    while (drop < n && at-s_history->samples[drop].at > BATTERY_HISTORY_WINDOW) ++drop;
    if (n-drop == BATTERY_HISTORY_MAX) ++drop;
    if (drop) { memmove(s_history->samples, s_history->samples+drop, (n-drop)*sizeof(battery_sample_t)); n -= drop; }
    s_history->samples[n++] = (battery_sample_t){at,(uint16_t)millivolts,(uint8_t)percent,charging?1:0};
    s_history->count = (uint32_t)n;
    // 未用项归零，使校验稳定且不泄露回收的数据。/ Zero unused entries for stable checksums and no stale data.
    memset(s_history->samples+n, 0, (BATTERY_HISTORY_MAX-n)*sizeof(battery_sample_t));
    save();
}
const battery_sample_t *battery_history_samples(size_t *count) {
    if (count) *count = s_history ? s_history->count : 0;
    return s_history ? s_history->samples : NULL;
}
