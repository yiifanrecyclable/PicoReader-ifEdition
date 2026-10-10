/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * 中文：绘制与命中紧凑四栏导航；Lucide 图标表示首页、内容、文件和设置。
 * English: Draw and hit-test compact four-tab navigation with Lucide home, contents, files and settings icons.
 */
#include "ui_nav.h"
#include "ui_nav_icons.h"
// Modified 2026-10-06: compact icon-only Lucide tabs and full local date/time in the status bar.
// 中文：状态栏左侧统一显示完整本地日期与时间，签名按可用宽度避让。
// 中文：选中导航铺满等宽矩形区域，使用深灰底白色图标。
// English: Selected navigation fills its equal-width rectangular quarter with dark gray and a white icon.
// 中文：底栏改用Lucide官方图标，入口名称按用户要求调整。
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "app_registry.h"
#include "read_pico_pmu.h"
#include "read_pico_pmu_protocol.h"
#include "read_pico_transfer.h"
#include "ui_kit.h"
#include "settings.h"
#define UI_NAV_NETWORK_ICON_PX 28
#define UI_STATUS_TIME_PX 24
#define UI_STATUS_PERCENT_PX 28
#define UI_STATUS_SIGNATURE_PX 21

void ui_nav_back(uint8_t *fb, int x, int y) {
    // 参考系统题头缩小返回键，中心仍保持在 y=113。/ Match the smaller reference back control while retaining the title centerline.
    ui_fill_round_rect(fb, (EpdRect){x, y + 13, 44, 44}, 22, UI_GRAY_WHITE);
    ui_draw_round_rect(fb, (EpdRect){x, y + 13, 44, 44}, 22, UI_NAV_BACK_BORDER_GRAY);
    ui_draw_round_rect(fb, (EpdRect){x + 1, y + 14, 42, 42}, 21, UI_NAV_BACK_BORDER_GRAY);
    ui_draw_icon(fb, x + 22, y + 35, 38, UI_ICON_CHEVRON_LEFT, UI_GRAY_BLACK);
}

void ui_nav_wifi_icon(uint8_t *fb, int cx, int cy, int size, uint8_t gray) {
    if (fb && size >= 16 && size <= 40) ui_draw_icon(fb, cx, cy, size, UI_ICON_WIFI, gray);
}

void ui_nav_draw(uint8_t *fb, int active) {
    epd_fill_rect((EpdRect){0, UI_NAV_TOP, UI_LOCK_WIDTH, UI_LOCK_HEIGHT - UI_NAV_TOP}, 0xe0, fb);
    ui_hairline(fb, UI_NAV_TOP, 0, UI_LOCK_WIDTH, UI_GRAY_LIGHT);
    for (int i = 0; i < 4; ++i) {
        int center = (i * 2 + 1) * UI_LOCK_WIDTH / 8;
        bool selected = i == active;
        if (selected) epd_fill_rect((EpdRect){i * UI_LOCK_WIDTH / 4, UI_NAV_TOP,
                                              UI_LOCK_WIDTH / 4, UI_NAV_HEIGHT}, UI_NAV_ACTIVE_GRAY, fb);
        static const ui_icon_t tabs[] = {UI_ICON_HOUSE, UI_ICON_LIBRARY_BIG,
                                       UI_ICON_FOLDER_OPEN, UI_ICON_SLIDERS_HORIZONTAL};
        ui_draw_icon(fb, center, UI_NAV_TOP + UI_NAV_HEIGHT / 2, 44, tabs[i],
                     selected ? UI_GRAY_WHITE : UI_GRAY_BLACK);
    }
}

// 空心电池内使用固定黑色实心闪电，低电量时仍清楚可见。
// A fixed solid black bolt remains visible inside the empty battery even at low charge.
static void ui_nav_battery_bolt(uint8_t *fb,int cx,int cy){
    epd_fill_triangle(cx+2,cy-9,cx-8,cy+1,cx-1,cy+1,UI_GRAY_BLACK,fb);
    epd_fill_triangle(cx+2,cy-9,cx-1,cy+1,cx+1,cy-2,UI_GRAY_BLACK,fb);
    epd_fill_triangle(cx-1,cy+1,cx-3,cy+9,cx+8,cy-2,UI_GRAY_BLACK,fb);
    epd_fill_triangle(cx-1,cy+1,cx+8,cy-2,cx+1,cy-2,UI_GRAY_BLACK,fb);
}

void ui_nav_status(uint8_t *fb) {
    time_t now = time(NULL);
    const pmu_snapshot_t *pmu = read_pico_pmu_get();
    if (now < 1704067200 && pmu && pmu->time_synced && pmu->unix_sec >= 1704067200)
        now = pmu->unix_sec;
    char clock[32] = "--:--";
    if (now > 1704067200) {
        time_t china = now + 8 * 3600;
        struct tm date;
        if (gmtime_r(&china, &date)) strftime(clock, sizeof(clock), "%Y-%m-%d %H:%M", &date);
    }
    const int status_center_y = 40;
    // 固定状态栏字号，让系统字体缩放时文字仍与图标共用中心线。
    // Keep status type fixed so text and glyphs share one centerline at every system font scale.
    ui_text_fixed_vc(fb, 36, status_center_y, UI_STATUS_TIME_PX, clock, EPD_DRAW_ALIGN_LEFT, false);
    read_pico_transfer_status_t network = {0};
    read_pico_transfer_get_status(&network);
    const bool wifi_visible = network.network_ready && network.mode == READ_PICO_TRANSFER_MODE_STA;
    char percent[8] = "--%";
    int gauge_percent = pmu_battery_percent(pmu);
    if (gauge_percent >= 0) snprintf(percent, sizeof(percent), "%u%%", (unsigned)gauge_percent);
    // 按实际百分比宽度排列网络图标，留 10 像素间隔；墨色与电池相同。
    // Place the network marks 10 px before the measured percentage, using the battery ink.
    const int percent_left = 595 - ui_text_fixed_width_px(UI_STATUS_PERCENT_PX, percent);
    const int network_right = percent_left - 10;
    const int wifi_center = network_right - UI_NAV_NETWORK_ICON_PX / 2;
    int signature_width = 280;
    const int clock_right = 36 + ui_text_fixed_width_px(UI_STATUS_TIME_PX, clock);
    int available = 2 * (UI_LOCK_WIDTH / 2 - clock_right - 12);
    if (available < signature_width) signature_width = available > 0 ? available : 0;
    int group_left = percent_left;
    if (wifi_visible) {
        group_left = wifi_center - UI_NAV_NETWORK_ICON_PX / 2;
    }
    available = 2 * (group_left - 12 - UI_LOCK_WIDTH / 2);
    if (available < signature_width) signature_width = available > 0 ? available : 0;
    char signature[96];
    snprintf(signature, sizeof(signature), "%s", app_settings_status_signature());
    while (signature[0] && ui_text_fixed_width_px(UI_STATUS_SIGNATURE_PX, signature) > signature_width) {
        size_t n = strlen(signature) - 1;
        while (n && ((unsigned char)signature[n] & 0xc0) == 0x80) --n;
        signature[n] = 0;
    }
    if (signature[0]) ui_text_fixed_vc(fb, UI_LOCK_WIDTH / 2, status_center_y, UI_STATUS_SIGNATURE_PX,
                                      signature, EPD_DRAW_ALIGN_CENTER, false);
    if (wifi_visible) ui_nav_wifi_icon(fb, wifi_center, status_center_y, UI_NAV_NETWORK_ICON_PX, UI_GRAY_BLACK);
    EpdRect battery = {606, status_center_y - 11, 40, 22};
    ui_fill_round_rect(fb,battery,6,UI_GRAY_WHITE);
    ui_draw_round_rect(fb, battery, 6, UI_GRAY_BLACK);
    ui_fill_round_rect(fb, (EpdRect){648, status_center_y - 4, 5, 8}, 2, UI_GRAY_BLACK);
    bool charging = pmu_battery_charging(pmu);
    if (!charging && gauge_percent >= 0) {
        int width = (31 * gauge_percent + 50) / 100;
        if (width > 0) ui_fill_round_rect(fb, (EpdRect){610, status_center_y - 7, width, 14}, 4, UI_GRAY_BLACK);
    }
    if (charging) ui_nav_battery_bolt(fb,627,status_center_y);
    ui_text_fixed_vc(fb, 595, status_center_y, UI_STATUS_PERCENT_PX, percent, EPD_DRAW_ALIGN_RIGHT, false);
}

int ui_nav_hit(uint16_t x, uint16_t y) {
    if (x >= UI_LOCK_WIDTH || y < UI_NAV_TOP || y >= UI_LOCK_HEIGHT) return -1;
    int tab = x * 4 / UI_LOCK_WIDTH;
    return tab < 4 ? tab : -1;
}

void ui_nav_request(app_ctx_t *ctx, int index) {
    if (index >= 0 && index < 4) ctx->request_app = app_at(index);
}
