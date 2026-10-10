/* SPDX-License-Identifier: Apache-2.0
 * 中文：坚果云目录多选和下载进度页，纯绘制读取一致会话快照。
 * English: Nutstore multi-selection and download progress, painted only from session snapshots.
 */
#include "app_cloud.h"
#include "app.h"
#include "app_registry.h"
#include "cloud_sync.h"
#include "read_pico_transfer.h"
#include "ui_gesture.h"
#include "ui_kit.h"
#include "ui_nav.h"
#include "ui_menu.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
static unsigned s_cloud_page, s_cloud_seen;
static int64_t s_cloud_last_draw;
static void cloud_fit(char *text, int px, int width) {
    while (*text && ui_text_fixed_width_px(ui_text_effective_px(px), text) > width) {
        size_t n = strlen(text) - 1;
        while (n && ((unsigned char)text[n] & 0xc0) == 0x80)
            --n;
        text[n] = 0;
    }
}
static void render(app_ctx_t *ctx, uint8_t *fb) {
    (void)ctx;
    cloud_status_t status;
    cloud_get_status(&status);
    ui_clear_page(fb);
    ui_nav_status(fb);
    ui_nav_back(fb, 36, 79);
    ui_text_vc(fb, 342, 113, 34, "坚果云同步", EPD_DRAW_ALIGN_CENTER, false);
    char folder[241];
    snprintf(folder, sizeof(folder), "%s", status.folder[0] ? status.folder : CLOUD_DEFAULT_FOLDER);
    cloud_fit(folder, 22, 190);
    ui_text_vc(fb, 36, 203, 22, folder, EPD_DRAW_ALIGN_LEFT, false);
    char info[96];
    snprintf(info, sizeof(info), "%s",
             status.indicator[0] ? status.indicator
             : status.configured ? "待读取目录"
                                 : "未配置账号");
    cloud_fit(info, 18, 280);
    ui_text_vc(fb, 530, 203, 18, info, EPD_DRAW_ALIGN_RIGHT, false);
    ui_draw_round_rect(fb, (EpdRect){550, 176, 98, 54}, 12, 0x70);
    ui_text_vc(fb, 599, 203, 20, "刷新", EPD_DRAW_ALIGN_CENTER, false);
    if (status.busy) {
        char text[160];
        snprintf(text, sizeof(text), "%s", status.message);
        cloud_fit(text, 25, 572);
        ui_text_vc(fb, 342, 390, 25, text, EPD_DRAW_ALIGN_CENTER, false);
        char count[64];
        if (status.total)
            snprintf(count, sizeof(count), "已入库 %u / %u", status.done, status.total);
        else
            snprintf(count, sizeof(count), "读取完成后可勾选下载");
        ui_text_vc(fb, 342, 454, 22, count, EPD_DRAW_ALIGN_CENTER, false);
        EpdRect bar = {60, 513, 564, 12};
        ui_fill_round_rect(fb, bar, 6, 0xc0);
        if (status.bytes_total) {
            bar.width =
                (int)(status.bytes_done > status.bytes_total ? 564
                                                             : status.bytes_done * 564 / status.bytes_total);
            if (bar.width)
                ui_fill_round_rect(fb, bar, 6, 0x40);
        }
        if (status.bytes_total) {
            snprintf(count, sizeof(count), "%llu / %llu KB", (unsigned long long)(status.bytes_done / 1024),
                     (unsigned long long)(status.bytes_total / 1024));
            ui_text_vc(fb, 342, 558, 20, count, EPD_DRAW_ALIGN_CENTER, false);
        }
        ui_draw_button(fb, (EpdRect){36, 985, 612, 78}, status.cancel_requested ? "正在停止…" : "取消同步",
                       false);
    } else {
        if (!status.count) {
            char text[160];
            snprintf(text, sizeof(text), "%s", status.message[0] ? status.message : "目录中还没有可读内容");
            cloud_fit(text, 23, 572);
            ui_text_vc(fb, 342, 378, 23, text, EPD_DRAW_ALIGN_CENTER, false);
            if (!status.configured) {
                ui_text_vc(fb, 342, 432, 21, "前往 设置 → 坚果云", EPD_DRAW_ALIGN_CENTER, false);
                ui_draw_button(fb, (EpdRect){36, 505, 612, 78}, "打开坚果云设置", false);
            }
        }
        unsigned selected = 0;
        for (unsigned i = 0; i < status.count; ++i) {
            cloud_entry_t entry;
            if (cloud_get_entry(i, &entry) && entry.selected)
                ++selected;
        }
        for (unsigned row = 0; row < 8; ++row) {
            unsigned i = s_cloud_page * 8 + row;
            cloud_entry_t entry;
            if (!cloud_get_entry(i, &entry))
                break;
            int y = 244 + row * 88;
            char name[241];
            snprintf(name, sizeof(name), "%s", entry.name);
            cloud_fit(name, 23, 510);
            ui_text(fb, 44, y + 10, 23, name, EPD_DRAW_ALIGN_LEFT, false);
            char detail[120];
            if (entry.error[0])
                snprintf(detail, sizeof(detail), "%s", entry.error);
            else if (entry.date_ms) {
                time_t t = entry.date_ms / 1000 + 8 * 3600;
                struct tm date;
                char day[16] = "";
                if (gmtime_r(&t, &date))
                    strftime(day, sizeof(day), "%Y-%m-%d", &date);
                snprintf(detail, sizeof(detail), "%s · %llu KB", day, (unsigned long long)(entry.bytes / 1024));
            } else
                snprintf(detail, sizeof(detail), "日期未知 · %llu KB",
                         (unsigned long long)(entry.bytes / 1024));
            cloud_fit(detail, 18, 510);
            ui_text(fb, 44, y + 49, 18, detail, EPD_DRAW_ALIGN_LEFT, false);
            EpdRect box = {590, y + 25, 34, 34};
            ui_draw_round_rect(fb, box, 7, entry.selectable ? 0x70 : 0xc0);
            if (entry.selected) {
                ui_fill_round_rect(fb, box, 7, UI_GRAY_BLACK);
                epd_draw_line(598, y + 42, 605, y + 48, UI_GRAY_WHITE, fb);
                epd_draw_line(605, y + 48, 618, y + 33, UI_GRAY_WHITE, fb);
            }
            ui_hairline(fb, y + 86, 44, 596, 0xc0);
        }
        char action[64];
        snprintf(action, sizeof(action), "同步所选（%u）", selected);
        ui_draw_button(fb, (EpdRect){36, 985, 612, 78}, action, selected > 0);
        if (status.count && status.message[0]) {
            char result[160];
            snprintf(result,sizeof(result),"%s",status.message);
            cloud_fit(result,19,612);
            ui_text_fixed_vc(fb,342,1096,19,result,EPD_DRAW_ALIGN_CENTER,false);
        }
        if (status.count) {
            char page[32];
            snprintf(page, sizeof(page), "%u / %u", s_cloud_page + 1, (status.count + 7) / 8);
            ui_text_vc(fb, 648, 964, 18, page, EPD_DRAW_ALIGN_RIGHT, false);
        }
    }
    ui_nav_draw(fb, 2);
}
static void enter(app_ctx_t *ctx) {
    s_cloud_page = s_cloud_seen = 0;
    s_cloud_last_draw = ctx->now_ms;
    cloud_refresh();
}
static void close_session(app_ctx_t *ctx) {
    (void)ctx;
    cloud_close();
}
static app_redraw_t tick(app_ctx_t *ctx) {
    read_pico_transfer_service_poll();
    cloud_status_t status;
    cloud_get_status(&status);
    if (status.revision == s_cloud_seen || ctx->now_ms - s_cloud_last_draw < 700)
        return APP_REDRAW_NONE;
    s_cloud_seen = status.revision;
    s_cloud_last_draw = ctx->now_ms;
    if (s_cloud_page * 8 >= status.count)
        s_cloud_page = 0;
    return APP_REDRAW_PAGE;
}
static app_redraw_t gesture(app_ctx_t *ctx, const ui_gesture_event_t *ev) {
    cloud_status_t status;
    cloud_get_status(&status);
    if (!status.busy && (ev->type == UI_GESTURE_SWIPE_U || ev->type == UI_GESTURE_SWIPE_L)) {
        if ((s_cloud_page + 1) * 8 < status.count)
            ++s_cloud_page;
        return APP_REDRAW_PAGE;
    }
    if (!status.busy && (ev->type == UI_GESTURE_SWIPE_D || ev->type == UI_GESTURE_SWIPE_R)) {
        if (s_cloud_page)
            --s_cloud_page;
        return APP_REDRAW_PAGE;
    }
    if (ev->type != UI_GESTURE_TAP)
        return APP_REDRAW_NONE;
    int tab = ui_nav_hit(ev->x0, ev->y0);
    if (tab >= 0) {
        cloud_cancel();
        ui_nav_request(ctx, tab);
        return APP_REDRAW_NONE;
    }
    if (ev->y0 < 170 && ev->x0 < 160) {
        cloud_cancel();
        ui_nav_request(ctx, 2);
        return APP_REDRAW_NONE;
    }
    if (status.busy) {
        if (ev->y0 >= 985 && ev->y0 < 1063)
            cloud_cancel();
        return APP_REDRAW_PAGE;
    }
    if (ev->y0 >= 176 && ev->y0 < 230 && ev->x0 >= 550) {
        s_cloud_page = 0;
        cloud_refresh();
        return APP_REDRAW_PAGE;
    }
    if (!status.configured && ev->y0 >= 505 && ev->y0 < 583) {
        extern const app_desc_t app_device_settings;
        app_device_settings_request_cloud_config();
        ctx->request_app = &app_device_settings;
        return APP_REDRAW_NONE;
    }
    if (ev->y0 >= 244 && ev->y0 < 948) {
        cloud_toggle(s_cloud_page * 8 + (ev->y0 - 244) / 88);
        return APP_REDRAW_PAGE;
    }
    if (ev->y0 >= 985 && ev->y0 < 1063) {
        cloud_download_selected();
        return APP_REDRAW_PAGE;
    }
    return APP_REDRAW_NONE;
}
static app_redraw_t key(app_ctx_t *ctx, int key) {
    cloud_cancel();
    ui_nav_request(ctx, key == UI_KEY_2 ? 0 : 2);
    return APP_REDRAW_NONE;
}
static bool no_menu(app_ctx_t *ctx) {
    (void)ctx;
    return false;
}
const app_desc_t app_cloud = {.title = "坚果云",
                              .detail = "勾选下载并离线阅读",
                              .owns_keys = true,
                              .menu_handle_enabled = no_menu,
                              .on_enter = enter,
                              .on_exit = close_session,
                              .on_before_lock = close_session,
                              .on_media_lost = close_session,
                              .render = render,
                              .on_tick = tick,
                              .on_gesture = gesture,
                              .on_key = key};
