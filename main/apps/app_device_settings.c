/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * Modified derivative: compact root layout and move firmware upgrade here.
 * 自定义固件修改：精简根页标题并承接固件升级；移除书架样式设置，显示分组改为五行。
 * IF change: Remove shelf-style settings and keep five display rows.
 *
 * 中文：分组设置页；网络短时对时后 PMU 持续走时，开机恢复系统时钟。
 * English: Grouped settings; temporary WiFi sync seeds the PMU, whose RTC restores time at boot.
 * 用户修订：设置列表滑动仅差分刷新内容，滑动过程中不周期插入黑白清屏。
 * User revision: settings lists scroll with content-only differentials and no periodic black/white wipe during swipes.
 */
#include <stdio.h>
#include <stdlib.h>
#include "esp_heap_caps.h"
#include <dirent.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>

#include "app.h"
#include "app_cloud.h"
#include "cloud_sync.h"
#include "app_content_open.h"
#include "app_registry.h"
#include "app_transfer_mode.h"
#include "settings.h"
#include "read_pico_pmu.h"
#include "read_pico_pmu_protocol.h"
#include "read_pico_transfer.h"
#include "esp_log.h"
#include "pmu_selftest.h"
#include "esp_system.h"
#include "soc/rtc_cntl_reg.h"
#include "ttf_font.h"
#include "app_font_context.h"
#include "ui_gesture.h"
#include "ui_kit.h"
#include "ui_text_input.h"
#include "ui_nav.h"
#include "ui_wallpaper.h"
#include "read_pico_search.h"
#include "book_store.h"
#include "book_ticket.h"
#include "content_library.h"
#include "ui_banner.h"
#include "ui_bitmap.h"
#include "app_content_open.h"
#include "ota_update.h"
#include "esp_app_desc.h"
#include "esp_attr.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "display.h"
#include "e0470_epaper_waveform.h"
#include "pmu_selftest.h"
#include "soc/rtc_cntl_reg.h"

static void fit_value(char *value, int width);
static char s_notice[96];
typedef enum { SETTINGS_MAIN, SETTINGS_WIFI, SETTINGS_TIME,
               SETTINGS_TIME_EDIT, SETTINGS_RETIRED_4, SETTINGS_SYSTEM_FONT,
               SETTINGS_SYSTEM_SIZE, SETTINGS_SYSTEM_CONTRAST, SETTINGS_LOCK_STYLE,
               SETTINGS_WALLPAPER, SETTINGS_WALLPAPER_PREVIEW,
               SETTINGS_CONFIG, SETTINGS_UPGRADE, SETTINGS_BOOT,
               SETTINGS_POWER_SLEEP, SETTINGS_RETIRED_15, SETTINGS_RETIRED_16,
               SETTINGS_RETIRED_17, SETTINGS_RETIRED_18, SETTINGS_TEXT_EDIT, SETTINGS_CLOUD, SETTINGS_HOME_QUOTE, SETTINGS_QUOTE_FONT } settings_page_t;
static settings_page_t s_page;
static int s_main_scroll, s_font_page, s_wallpaper_page;
static bool s_scroll_drag_consumed, s_scroll_present_pending;
static bool s_sync_pending;
static bool s_config_confirm;
static bool s_boot_pending;
static bool s_upgrade_confirm, s_local_pending, s_upgrade_restart;
static pico_ota_info_t s_local_update;
static char s_upgrade_notice[96];
static void upgrade_open(void) {
    (void)pico_ota_inspect(PICO_OTA_UPDATE_PATH, &s_local_update);
    s_upgrade_confirm = s_local_pending = s_upgrade_restart = false;
    s_upgrade_notice[0] = 0;
    s_page = SETTINGS_UPGRADE;
}
static const char *const TAG = "device_settings";
#define SETTINGS_ROW_H 68
#define SETTINGS_GROUP_GAP 66
#define SETTINGS_WIRELESS_ROWS 2
#define SETTINGS_DISPLAY_ROWS 5
#define SETTINGS_DEVICE_ROWS 3
#define SETTINGS_MAINTENANCE_ROWS 3
#define SETTINGS_WIRELESS_Y 235
// 分组布局从上一组底边推导，绘制与命中共用坐标。/ Derive groups from the previous bottom; drawing and hits share coordinates.
#define SETTINGS_DISPLAY_Y (SETTINGS_WIRELESS_Y + SETTINGS_WIRELESS_ROWS * SETTINGS_ROW_H + SETTINGS_GROUP_GAP)
#define SETTINGS_DEVICE_Y (SETTINGS_DISPLAY_Y + SETTINGS_DISPLAY_ROWS * SETTINGS_ROW_H + SETTINGS_GROUP_GAP)
#define SETTINGS_MAINTENANCE_Y (SETTINGS_DEVICE_Y + SETTINGS_DEVICE_ROWS * SETTINGS_ROW_H + SETTINGS_GROUP_GAP)
#define SETTINGS_ICON_PX 32
#define SETTINGS_ICON_INK 0x58
#define SETTINGS_SCROLL_MAX (SETTINGS_MAINTENANCE_Y + SETTINGS_MAINTENANCE_ROWS * SETTINGS_ROW_H - UI_NAV_TOP + 12)
static char s_editor[257], s_editor_notice[80];
static ui_text_input_t s_text_input;
static bool s_home_quote_editor, s_home_quote_requested;
void app_device_settings_request_home_quote(void){s_home_quote_requested=true;}
bool app_device_settings_home_quote_requested(void){return s_home_quote_requested;}
static int s_cloud_editor;
static bool s_cloud_requested;
void app_device_settings_request_cloud_config(void){s_cloud_requested=true;}
bool app_device_settings_cloud_requested(void){return s_cloud_requested;}
static uint8_t *s_quote_preview;
static int s_quote_px;
static bool s_quote_dragging,s_quote_preview_attempted,s_quote_committed;
bool app_device_settings_quote_committed(void){return s_quote_committed;}
int app_device_settings_quote_size(void){return s_quote_px?s_quote_px:content_home_quote_size();}
static void quote_preview_clear(void){free(s_quote_preview);s_quote_preview=NULL;s_quote_preview_attempted=false;}
// 只在进入/提交/松手时生成字库效果；拖动只刷新滑块，避免反复加载TF字库和写NVS。
// Rasterize on entry/commit/release; moves update only the slider without repeated TF loads or NVS writes.
static bool quote_preview_prepare_alignment(int px,bool save,content_quote_align_t align,bool save_align){
    uint32_t days[30],day=0;bool valid=book_ticket_recent_days(days,&day);s_quote_preview_attempted=true;
#ifdef PICO_HOST_SIMULATOR
    const char *sample=getenv("PICO_SIM_HOME_DAY");if(sample){unsigned long d=strtoul(sample,NULL,10);if(d>=19723&&d<50000){day=(uint32_t)d;valid=true;}}
#endif
    uint8_t *image=NULL;bool fits=false;
    if(!ui_banner_preview(content_home_quote(),content_home_quote_font(),px,align,day,valid,&image,&fits)){
        strlcpy(s_notice,"预览暂不可用，请重试",sizeof(s_notice));return false;}
    if((save||save_align)&&!fits){free(image);strlcpy(s_notice,"字号过大，当前文字超过两行",sizeof(s_notice));return false;}
    if(save&&content_home_quote_size_set((unsigned)px)!=ESP_OK){free(image);strlcpy(s_notice,"字号保存失败，请重试",sizeof(s_notice));return false;}
    if(save_align&&content_home_quote_alignment_set((unsigned)align)!=ESP_OK){free(image);strlcpy(s_notice,"排版保存失败，请重试",sizeof(s_notice));return false;}
    free(s_quote_preview);s_quote_preview=image;s_quote_px=px;
    strlcpy(s_notice,fits?"":"当前文字超出两行，请修改文字",sizeof(s_notice));
    if(save||save_align)s_quote_committed=true;
    return true;
}
static bool quote_preview_prepare(int px,bool save){return quote_preview_prepare_alignment(px,save,content_home_quote_alignment(),false);}
static EpdRect quote_alignment_rect(int i){return (EpdRect){36+156*i,978,144,56};}
// 三条精简横线表示对齐方式；用原生几何绘制，避免引入四份位图资源。
// Three compact strokes show alignment; native geometry avoids four bitmap assets.
static void quote_alignment_draw(uint8_t *fb,int option,bool selected){
    EpdRect r=quote_alignment_rect(option);
    if(selected)ui_fill_round_rect(fb,r,16,UI_NAV_ACTIVE_GRAY);
    int cx=r.x+r.width/2,cy=r.y+r.height/2;
    static const int lengths[]={44,28,36};
    for(int row=0;row<3;++row){
        int width=option==CONTENT_QUOTE_JUSTIFY?44:lengths[row];
        int x=option==CONTENT_QUOTE_LEFT?cx-22:option==CONTENT_QUOTE_RIGHT?cx+22-width:cx-width/2;
        ui_fill_round_rect(fb,(EpdRect){x,cy-17+row*15,width,4},2,selected?UI_GRAY_WHITE:0x38);
    }
}
static int quote_slider_size(int x){
    if(x<72)x=72;
    if(x>612)x=612;
    return CONTENT_QUOTE_SIZE_MIN+2*(((x-72)*36+270)/540);
}
static app_redraw_t quote_slider_gesture(const ui_gesture_event_t *ev,bool *handled){
    bool hit=ui_rect_hit((EpdRect){36,812,612,90},ev->x0,ev->y0);
    *handled=s_quote_dragging||hit;if(!*handled)return APP_REDRAW_NONE;
    if(ev->type==UI_GESTURE_CANCEL){s_quote_dragging=false;s_quote_px=content_home_quote_size();return APP_REDRAW_AREA;}
    if(ev->type==UI_GESTURE_PRESS||ev->type==UI_GESTURE_MOVE){
        s_quote_dragging=true;int px=quote_slider_size(ev->x);if(px==s_quote_px)return APP_REDRAW_NONE;s_quote_px=px;return APP_REDRAW_AREA;
    }
    if(ev->type==UI_GESTURE_LONG_PRESS)return APP_REDRAW_NONE;
    bool release=ev->type==UI_GESTURE_TAP||ev->type==UI_GESTURE_SWIPE_L||ev->type==UI_GESTURE_SWIPE_R||ev->type==UI_GESTURE_SWIPE_U||ev->type==UI_GESTURE_SWIPE_D;
    if(!release)return APP_REDRAW_NONE;
    s_quote_dragging=false;int px=quote_slider_size(ev->x);
    if(!quote_preview_prepare(px,true))s_quote_px=content_home_quote_size();
    return APP_REDRAW_PAGE;
}

static const char *system_font_label(const char *path) {
    if (!path || !path[0]) return "思源黑体";
    const char *name = strrchr(path, '/');
    name = name ? name + 1 : path;
    static char stem[TTF_FONT_NAME_MAX];
    snprintf(stem, sizeof(stem), "%s", name);
    size_t len = strlen(stem);
    if (len >= 4 && !strcasecmp(stem + len - 4, ".ttf")) stem[len - 4] = 0;
    if (!strcasecmp(stem, "Hei")) return "思源黑体（TF 卡）";
    return ttf_font_localized_name(stem);
}
static int s_year, s_month, s_day, s_hour, s_minute;
#define WALLPAPER_MAX 64
typedef struct { char path[288]; char name[96]; } wallpaper_item_t;
// 壁纸列表放 PSRAM，理由同上。
// The wallpaper list lives in PSRAM for the same reason.
static wallpaper_item_t *s_wallpapers;

static bool wallpapers_alloc(void) {
    if (!s_wallpapers)
        s_wallpapers = heap_caps_calloc(WALLPAPER_MAX, sizeof(wallpaper_item_t),
                                        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    return s_wallpapers != NULL;
}
static int s_wallpaper_count;
static int s_wallpaper_selected;
static bool s_wallpaper_confirm, s_wallpaper_preview_ok;

static void wallpaper_scan_dir(const char *root) {
    if (!wallpapers_alloc()) return;
    DIR *dir = opendir(root);
    if (!dir) return;
    struct dirent *entry;
    while (s_wallpaper_count < WALLPAPER_MAX && (entry = readdir(dir))) {
        if (entry->d_name[0] == '.' || !strncmp(entry->d_name, "._", 2)) continue;
        const char *ext = strrchr(entry->d_name, '.');
        if (!ext || (strcasecmp(ext, ".jpg") && strcasecmp(ext, ".jpeg") && strcasecmp(ext, ".png"))) continue;
        wallpaper_item_t *item = &s_wallpapers[s_wallpaper_count];
        if (snprintf(item->path, sizeof(item->path), "%s/%s", root, entry->d_name) >= sizeof(item->path)) continue;
        struct stat st;
        if (stat(item->path, &st) || !S_ISREG(st.st_mode) || st.st_size <= 0 || st.st_size > 2 * 1024 * 1024) continue;
        size_t name_len = strnlen(entry->d_name, sizeof(item->name) - 1);
        memcpy(item->name, entry->d_name, name_len);
        item->name[name_len] = 0;
        ++s_wallpaper_count;
    }
    closedir(dir);
}
static void wallpaper_scan(void) {
    s_wallpaper_count = s_wallpaper_page = 0;
    wallpaper_scan_dir("/sdcard/pictures");
    wallpaper_scan_dir("/sdcard/images");
    wallpaper_scan_dir("/sdcard");
}

static void profile_editor_open(bool signature) {
    s_home_quote_editor=false;
    s_cloud_editor=0; s_editor_notice[0]=0;
    snprintf(s_editor,sizeof(s_editor),"%s",signature?app_settings_status_signature():"");
    ui_text_input_open(&s_text_input,s_editor,96,true,false,UI_INPUT_CHINESE);
    s_page=SETTINGS_TEXT_EDIT;
}
static void home_quote_editor_open(void) {
    profile_editor_open(false); s_home_quote_editor=true;
    strlcpy(s_editor,content_home_quote(),sizeof(s_editor));
    ui_text_input_open(&s_text_input,s_editor,CONTENT_HOME_QUOTE_BYTES,true,false,UI_INPUT_CHINESE);
}
static settings_page_t editor_return_page(void){return s_cloud_editor?SETTINGS_CLOUD:SETTINGS_MAIN;}
static void cloud_editor_open(int field) {
    profile_editor_open(false); s_cloud_editor=field; cloud_config_t cfg; cloud_config_load(&cfg);
    snprintf(s_editor,sizeof(s_editor),"%s",field==1?cfg.folder:field==2?cfg.account:"");
    memset(cfg.password,0,sizeof(cfg.password));
    ui_text_input_open(&s_text_input,s_editor,field==1?241:field==2?129:257,
        field==1,field==3,field==1?UI_INPUT_CHINESE:UI_INPUT_ENGLISH);
}

static int days_in_month(int year, int month) {
    static const int days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    if (month == 2 && (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0))) return 29;
    return days[month - 1];
}

// 公历日期转 UTC 天数；界面统一按北京时间 UTC+8 编辑。
// Convert a Gregorian date to UTC days; the editor uses Beijing time (UTC+8).
static int64_t civil_days(int year, int month, int day) {
    year -= month <= 2;
    int era = year / 400;
    int yoe = year - era * 400;
    int doy = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
    int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return (int64_t)era * 146097 + doe - 719468;
}

static void time_open(void) {
    time_t now = time(NULL);
    if (now < 1704067200) {
        const pmu_snapshot_t *pmu = read_pico_pmu_get();
        if (pmu && pmu->time_synced && pmu->unix_sec >= 1704067200) now = pmu->unix_sec;
    }
    if (now < 1704067200) {
        char month[4] = "Jan"; int day = 1, year = 2024;
        if (sscanf(__DATE__, "%3s %d %d", month, &day, &year) != 3) {
            strcpy(month, "Jan"); day = 1; year = 2024;
        }
        static const char *const months[] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
        int m = 1; while (m < 12 && strcmp(month, months[m - 1])) ++m;
        s_year = year; s_month = m; s_day = day; s_hour = s_minute = 0;
    } else {
        struct tm date;
        time_t local = now + 8 * 3600;
        gmtime_r(&local, &date);
        s_year = date.tm_year + 1900; s_month = date.tm_mon + 1; s_day = date.tm_mday;
        s_hour = date.tm_hour; s_minute = date.tm_min;
    }
    s_notice[0] = 0;
    s_page = SETTINGS_TIME_EDIT;
}

static void time_adjust(int field, int step) {
    if (field == 0) s_year = s_year + step > 2099 ? 2024 : s_year + step < 2024 ? 2099 : s_year + step;
    if (field == 1) s_month = s_month + step > 12 ? 1 : s_month + step < 1 ? 12 : s_month + step;
    if (field == 2) s_day = s_day + step > days_in_month(s_year, s_month) ? 1 :
                            s_day + step < 1 ? days_in_month(s_year, s_month) : s_day + step;
    if (field == 3) s_hour = (s_hour + step + 24) % 24;
    if (field == 4) s_minute = (s_minute + step + 60) % 60;
    if (s_day > days_in_month(s_year, s_month)) s_day = days_in_month(s_year, s_month);
}

static bool time_save(void) {
    int64_t epoch = civil_days(s_year, s_month, s_day) * 86400 +
                    s_hour * 3600 + s_minute * 60 - 8 * 3600;
    if (epoch < 1704067200 || epoch > 4102444799LL) return false;
    struct timeval tv = {.tv_sec = (time_t)epoch, .tv_usec = 0};
    if (settimeofday(&tv, NULL)) return false;
    uint8_t payload[4] = {(uint8_t)epoch, (uint8_t)(epoch >> 8),
                          (uint8_t)(epoch >> 16), (uint8_t)(epoch >> 24)};
    esp_err_t err = read_pico_pmu_cmd(PMU_CMD_TIME_SYNC, payload, sizeof(payload));
    if (err == ESP_OK) err = read_pico_pmu_cmd(PMU_CMD_TIME_GET, NULL, 0);
    const pmu_snapshot_t *pmu = read_pico_pmu_get();
    if (err != ESP_OK || !pmu || !pmu->time_synced) {
        snprintf(s_notice, sizeof(s_notice), "系统时间已设置，PMU 同步失败");
        return false;
    }
    snprintf(s_notice, sizeof(s_notice), "时间已保存，并同步到电源管理芯片");
    s_page = SETTINGS_TIME;
    return true;
}

static void settings_card(uint8_t *fb, EpdRect rect, int radius, uint8_t fill, uint8_t edge) {
    ui_fill_round_rect(fb, rect, radius, fill);
    ui_draw_round_rect(fb, rect, radius, edge);
    ui_draw_round_rect(fb, (EpdRect){rect.x + 1, rect.y + 1,
                                     rect.width - 2, rect.height - 2}, radius - 1, edge);
}
static void settings_divider(uint8_t *fb, int y, int x, int width) {
    epd_fill_rect((EpdRect){x, y, width, 2}, 0x78, fb);
}
static void row(uint8_t *fb, int y, const char *label, const char *value) {
    ui_text(fb, 54, y + 17, 27, label, EPD_DRAW_ALIGN_LEFT, false);
    ui_text(fb, 627, y + 20, 21, value, EPD_DRAW_ALIGN_RIGHT, false);
    settings_divider(fb, y + 68, 54, 575);
}
static void section(uint8_t *fb, int y, const char *title) {
    ui_text(fb, 40, y, 22, title, EPD_DRAW_ALIGN_LEFT, false);
}

/* ---- 蓝牙翻页器子页的版式 / Layout of the Bluetooth sub-page ---- */

static void back_header(uint8_t *fb, const char *title) {
    ui_nav_back(fb, 36, 79);
    ui_text_vc(fb, 342, 107, 34, title, EPD_DRAW_ALIGN_CENTER, false);
    settings_divider(fb, 174, 36, 612);
}

static void setting_icon(uint8_t *fb, int index, int cx, int cy) {
    // 每个设置项对应一个 Lucide 图标；索引顺序由各分组传入的 icons[] 数组决定。
    // One Lucide icon per setting row; the index order comes from each group's icons[] array.
    static const ui_icon_t icons[] = {
        UI_ICON_WIFI,              // 0 无线连接
        UI_ICON_WIFI,              // 1 保留图标索引 / Reserved icon index
        UI_ICON_TYPE,              // 2 系统字体、阅读操作
        UI_ICON_A_LARGE_SMALL,     // 3 系统字号
        UI_ICON_LIBRARY_BIG,       // 4 保留图标索引 / Reserved icon index
        UI_ICON_TICKET,            // 5 锁屏样式
        UI_ICON_CLOCK,             // 6 日期与时间
        UI_ICON_CONTRAST,          // 7 系统对比度
        UI_ICON_COPY,              // 8 保存与恢复
        UI_ICON_POWER,             // 9 关机睡眠
        UI_ICON_TEXT_ALIGN_START,  // 10 状态栏签名
        UI_ICON_REFRESH_CW,        // 11 首页强刷
        UI_ICON_NUTSTORE,          // 12 坚果云同步 / Nutstore sync
    };
    if (index < 0 || index >= (int)(sizeof(icons) / sizeof(icons[0]))) return;
    ui_draw_icon(fb, cx, cy, SETTINGS_ICON_PX, icons[index], SETTINGS_ICON_INK);
}

static void fit_value(char *value, int width) {
    while (*value && ttf_text_width_px(ui_text_effective_px(20), value) > width) {
        size_t n = strlen(value) - 1;
        while (n && ((unsigned char)value[n] & 0xc0) == 0x80) --n;
        value[n] = 0;
    }
}

static void setting_group(uint8_t *fb, int title_y, const char *title,
                          int card_y, const int icons[], const char *const labels[],
                          const char *const values[], int count) {
    const int row_height = SETTINGS_ROW_H;
    title_y -= s_main_scroll;
    card_y -= s_main_scroll;
    ui_text(fb, 42, title_y, 20, title, EPD_DRAW_ALIGN_LEFT, false);
    settings_card(fb, (EpdRect){36, card_y, 612, count * row_height}, 22,
                  UI_GRAY_WHITE, 0x70);
    for (int i = 0; i < count; ++i) {
        int y = card_y + i * row_height;
        if (i) settings_divider(fb, y, 88, 544);
        setting_icon(fb, icons[i], 67, y + row_height / 2);
        ui_text(fb, 100, y + 19, 25, labels[i], EPD_DRAW_ALIGN_LEFT, false);
        ui_text(fb, 619, y + 22, 20, values[i], EPD_DRAW_ALIGN_RIGHT, false);
    }
}

// 中文：说明按实际界面字形宽度折行，续行与正文对齐。/ English: wrap instructions to measured UI glyph width with hanging bullet indentation.
static int cloud_guide_bullet(uint8_t *fb, int y, const char *text) {
    ui_text(fb, 44, y, 21, "•", EPD_DRAW_ALIGN_LEFT, false);
    while (*text) {
        char line[192] = {0};
        size_t used = 0;
        const char *end = text;
        while (*end) {
            const char *next = end + 1;
            while (((unsigned char)*next & 0xc0) == 0x80) ++next;
            size_t bytes = (size_t)(next - end);
            if (used + bytes >= sizeof(line)) break;
            memcpy(line + used, end, bytes);
            line[used + bytes] = 0;
            if (used && ui_text_fixed_width_px(ui_text_effective_px(21), line) > 578) {
                line[used] = 0;
                break;
            }
            used += bytes;
            end = next;
        }
        if (!used) break;
        ui_text(fb, 66, y, 21, line, EPD_DRAW_ALIGN_LEFT, false);
        y += 30;
        text = end;
    }
    return y + 8;
}

static void render(app_ctx_t *ctx, uint8_t *fb) {
    (void)ctx;
    ui_clear_page(fb);
    if (s_page == SETTINGS_MAIN)
        epd_fill_rect((EpdRect){0, 0, UI_LOCK_WIDTH, UI_NAV_TOP}, 0xe0, fb);
    ui_nav_status(fb);
    if(s_page==SETTINGS_CLOUD){
        cloud_config_t cfg;cloud_config_load(&cfg);back_header(fb,"坚果云同步");
        char folder[241],account[129];snprintf(folder,sizeof(folder),"%s",cfg.folder);snprintf(account,sizeof(account),"%s",cfg.account[0]?cfg.account:"未设置");fit_value(folder,330);fit_value(account,330);
        row(fb,254,"同步文件夹",folder);row(fb,354,"账号",account);row(fb,454,"应用密码",cloud_config_valid(&cfg,true)?"已保存  ›":"未设置  ›");memset(cfg.password,0,sizeof(cfg.password));
        ui_text(fb,44,612,27,"使用说明",EPD_DRAW_ALIGN_LEFT,false);
        const char *guide[]={
            "同步文件夹填写坚果云根目录下已存在的文件夹",
            "账号为坚果云账号",
            "应用密码不是账户密码，在 设置-第三方应用管理 中添加生成",
            "联网状态下文件夹中的md、txt、epub文件会被Pico识别",
            "建议使用Agent管理电脑端的该文件夹",
            "obsidian端一键推送插件可联系作者获取",
            "设置页「保存与恢复」功能会将本页设置一并备份"
        };
        int guide_y = 660;
        for(size_t i=0;i<sizeof(guide)/sizeof(guide[0]);++i)
            guide_y = cloud_guide_bullet(fb, guide_y, guide[i]);
        if(s_notice[0])ui_text(fb,44,1100,20,s_notice,EPD_DRAW_ALIGN_LEFT,false);
        ui_nav_draw(fb,3);return;
    }
    if (s_page == SETTINGS_UPGRADE) {
        back_header(fb, "系统升级");
        settings_card(fb, (EpdRect){36, 202, 612, 120}, 22, UI_GRAY_WHITE, 0x70);
        ui_text(fb, 60, 224, 22, "当前版本", EPD_DRAW_ALIGN_LEFT, false);
        ui_text(fb, 60, 264, 29, esp_app_get_description()->version, EPD_DRAW_ALIGN_LEFT, false);
        settings_card(fb, (EpdRect){36, 356, 612, 344}, 22, UI_GRAY_WHITE, 0x70);
        ui_text(fb, 60, 383, 29, s_upgrade_confirm ? "确认 IF Edition 升级" : "TF 卡升级", EPD_DRAW_ALIGN_LEFT, false);
        ui_text(fb, 60, 447, 23, "根目录：Pico-IF-update.bin", EPD_DRAW_ALIGN_LEFT, false);
        ui_text(fb, 60, 510, 23, s_local_update.ready ? s_local_update.candidate_version : "请放入 IF Edition 升级包", EPD_DRAW_ALIGN_LEFT, false);
        if (s_upgrade_confirm) {
            ui_text(fb, 60, 559, 21, "升级期间请保持供电", EPD_DRAW_ALIGN_LEFT, false);
            ui_draw_button(fb, (EpdRect){60, 613, 260, 64}, "取消", false);
            ui_draw_button(fb, (EpdRect){364, 613, 260, 64}, "确认升级", true);
        } else {
            ui_draw_button(fb, (EpdRect){60, 613, 564, 64}, s_local_pending ? "正在安装，请勿断电" : s_local_update.ready ? "安装 TF 卡升级包" : "重新检查 TF 卡", s_local_update.ready);
        }
        if (s_upgrade_notice[0]) ui_text(fb, 48, 760, 21, s_upgrade_notice, EPD_DRAW_ALIGN_LEFT, false);
        ui_nav_draw(fb, 3); return;
    }
    if(s_page==SETTINGS_HOME_QUOTE){
        back_header(fb,"首页文字");
        ui_text_fixed_vc(fb,36,205,28,"效果预览",EPD_DRAW_ALIGN_LEFT,false);
        if(s_quote_preview)ui_bitmap_draw(fb,(EpdRect){0,230,UI_LOCK_WIDTH,300},s_quote_preview);
        else ui_text_fixed_vc(fb,36,358,22,"预览暂不可用",EPD_DRAW_ALIGN_LEFT,false);
        settings_card(fb,(EpdRect){36,572,612,72},18,UI_GRAY_WHITE,0x70);
        ui_text_vc(fb,60,608,28,"修改文字",EPD_DRAW_ALIGN_LEFT,false);
        settings_card(fb,(EpdRect){36,662,612,72},18,UI_GRAY_WHITE,0x70);
        ui_text_vc(fb,60,698,28,"选择字体",EPD_DRAW_ALIGN_LEFT,false);
        char font[96];snprintf(font,sizeof(font),"%s",content_home_quote_font()[0]?system_font_label(content_home_quote_font()):"跟随系统字体");fit_value(font,270);
        ui_text_vc(fb,624,698,20,font,EPD_DRAW_ALIGN_RIGHT,false);
        ui_text_fixed_vc(fb,36,778,30,"修改字号",EPD_DRAW_ALIGN_LEFT,false);
        char size[32];snprintf(size,sizeof(size),"%d px",s_quote_px);ui_text_fixed_vc(fb,648,778,30,size,EPD_DRAW_ALIGN_RIGHT,false);
        epd_fill_rect((EpdRect){72,846,540,4},0xc0,fb);int knob=72+(s_quote_px-CONTENT_QUOTE_SIZE_MIN)*540/72;
        epd_fill_rect((EpdRect){72,846,knob-72,4},0x44,fb);epd_fill_circle(knob,848,12,0x44,fb);
        ui_text_fixed_vc(fb,36,548,20,s_notice[0]?s_notice:"拖动调整字号，最多显示两行",EPD_DRAW_ALIGN_LEFT,false);
        ui_text_fixed_vc(fb,36,946,30,"文字排版",EPD_DRAW_ALIGN_LEFT,false);
        content_quote_align_t align=content_home_quote_alignment();
        for(int i=0;i<4;++i)quote_alignment_draw(fb,i,align==(content_quote_align_t)i);
        ui_draw_button(fb,(EpdRect){36,1064,612,56},"完成",true);
        ui_nav_draw(fb,0);return;
    }
    if (s_page == SETTINGS_TEXT_EDIT) {
        ui_text_input_draw(fb,&s_text_input,
            s_home_quote_editor?"首页格言":s_cloud_editor==1?"同步文件夹":s_cloud_editor==2?"坚果云账号":s_cloud_editor==3?"应用密码":"状态栏签名",
            s_home_quote_editor?"最多两行，超出请缩短后保存；留空隐藏":s_cloud_editor?"应用密码独立于坚果云登录密码":"状态栏中间显示，留空则隐藏",
            NULL,"完成",s_editor_notice);
        return;
    }
    if (s_page == SETTINGS_TIME_EDIT) {
        ui_nav_back(fb, 36, 79);
        ui_text(fb, 36, 151, 47, "日期与时间", EPD_DRAW_ALIGN_LEFT, false);
        ui_text(fb, 36, 216, 23, "北京时间 · UTC+8", EPD_DRAW_ALIGN_LEFT, false);
        ui_hairline(fb, 259, 36, 612, UI_GRAY_LIGHT);
        static const char *const labels[] = {"年", "月", "日", "时", "分"};
        int values[] = {s_year, s_month, s_day, s_hour, s_minute};
        for (int i = 0; i < 5; ++i) {
            int x = 36 + i * 122;
            ui_text(fb, x + 52, 300, 24, labels[i], EPD_DRAW_ALIGN_CENTER, false);
            ui_draw_round_rect(fb, (EpdRect){x, 354, 106, 62}, 8, UI_GRAY_BLACK);
            ui_text_vc(fb, x + 53, 385, 30, "+", EPD_DRAW_ALIGN_CENTER, false);
            char value[8]; snprintf(value, sizeof(value), i == 0 ? "%04d" : "%02d", values[i]);
            ui_text(fb, x + 53, 456, i == 0 ? 34 : 39, value, EPD_DRAW_ALIGN_CENTER, false);
            ui_draw_round_rect(fb, (EpdRect){x, 535, 106, 62}, 8, UI_GRAY_BLACK);
            ui_text_vc(fb, x + 53, 566, 30, "−", EPD_DRAW_ALIGN_CENTER, false);
        }
        ui_text(fb, 36, 649, 23, "点上方 + 或下方 − 调整数字", EPD_DRAW_ALIGN_LEFT, false);
        ui_draw_button(fb, (EpdRect){36, 767, 612, 78}, "保存时间", false);
        ui_draw_button(fb, (EpdRect){36, 874, 612, 78}, "取消", false);
        if (s_notice[0]) ui_text(fb, 36, 996, 22, s_notice, EPD_DRAW_ALIGN_LEFT, false);
        return;
    }
    if (s_page == SETTINGS_WIFI) {
        back_header(fb, "WiFi");
        char ssid[33] = {0}; bool saved = false;
        (void)read_pico_transfer_get_saved_wifi(ssid, &saved);
        section(fb, 253, "已保存网络");
        row(fb, 296, saved ? ssid : "尚未配置", saved ? "已保存" : "");
        row(fb, 408, "连接或更改网络", "前往传输  ›");
        ui_text(fb, 54, 529, 21, "配网后可在日期与时间中进行 WiFi 对时。", EPD_DRAW_ALIGN_LEFT, false);
        ui_nav_draw(fb, 3);
        return;
    }
    if (s_page == SETTINGS_SYSTEM_FONT || s_page == SETTINGS_QUOTE_FONT) {
        back_header(fb, s_page==SETTINGS_QUOTE_FONT?"首页文字字体":"系统字体");
        section(fb, 244, s_page==SETTINGS_QUOTE_FONT?(s_notice[0]?s_notice:"只影响首页第三行文字"):"只影响首页、书架和设置等系统界面");
        const char *selected = s_page==SETTINGS_QUOTE_FONT?content_home_quote_font():app_settings_system_font_path();
        int count = ttf_font_count();
        int first = s_font_page * 8;
        for (int slot = 0; slot < 8; ++slot) {
            int item_index = first + slot;
            if (item_index > count) break;
            EpdRect box = {36, 292 + slot * 92, 612, 78};
            const ttf_font_item_t *item = item_index ? ttf_font_item(item_index - 1) : NULL;
            bool active = item ? !strcmp(selected, item->path) : !selected[0];
            settings_card(fb, box, 18, active ? 0xd0 : UI_GRAY_WHITE, 0x70);
            ui_draw_round_rect(fb, box, 18, active ? 0x90 : 0xd0);
            char label[TTF_FONT_NAME_MAX];
            snprintf(label, sizeof(label), "%s", item ? system_font_label(item->path) : s_page==SETTINGS_QUOTE_FONT?"跟随系统字体":"思源黑体（内建）");
            while (label[0] && ttf_text_width_px(ui_text_effective_px(26), label) > box.width - 102) {
                size_t len = strlen(label) - 1;
                while (len && ((unsigned char)label[len] & 0xc0) == 0x80) --len;
                label[len] = 0;
            }
            ui_text_vc(fb, 60, box.y + 39, 26, label, EPD_DRAW_ALIGN_LEFT, false);
            if (active) epd_fill_circle(609, box.y + 39, 6, UI_GRAY_BLACK, fb);
        }
        char pages[32]; snprintf(pages, sizeof(pages), "%d / %d", s_font_page + 1, (count + 8) / 8);
        ui_text(fb, 648, 1050, 20, pages, EPD_DRAW_ALIGN_RIGHT, false);
        ui_nav_draw(fb, s_page==SETTINGS_QUOTE_FONT?0:3);
        return;
    }
    if (s_page == SETTINGS_SYSTEM_SIZE) {
        back_header(fb, "系统字号");
        section(fb, 248, "界面文字大小");
        for (int i = 0; i < 11; ++i) {
            int value = 100 + i * 10;
            EpdRect box = {36 + (i % 2) * 316, 298 + (i / 2) * 122, 296, 102};
            bool active = app_settings_system_font_size() == value;
            settings_card(fb, box, 20, active ? 0xd0 : UI_GRAY_WHITE, 0x70);
            char label[48]; snprintf(label, sizeof(label), "%d%%", value);
            ui_text_vc(fb, box.x + 24, box.y + 51, 29, label, EPD_DRAW_ALIGN_LEFT, false);
            if (active) epd_fill_circle(box.x + box.width - 25, box.y + 51, 8, UI_GRAY_BLACK, fb);
        }
        ui_nav_draw(fb, 3);
        return;
    }
    if (s_page == SETTINGS_SYSTEM_CONTRAST) {
        back_header(fb, "系统对比度");
        section(fb, 248, "调整系统界面的灰阶层次与清晰度");
        static const char *const names[] = {"柔和", "标准", "清晰", "鲜明", "高对比"};
        for (int i = 0; i < 5; ++i) {
            int value = 100 + i * 10;
            EpdRect box = {36, 298 + i * 116, 612, 90};
            bool active = app_settings_system_contrast() == value;
            settings_card(fb, box, 20, active ? 0xd0 : UI_GRAY_WHITE, 0x70);
            ui_text_vc(fb, 64, box.y + 45, 28, names[i], EPD_DRAW_ALIGN_LEFT, false);
            char shown[16]; snprintf(shown, sizeof(shown), "%d%%", value);
            ui_text_vc(fb, 570, box.y + 45, 22, shown, EPD_DRAW_ALIGN_RIGHT, false);
            if (active) epd_fill_circle(605, box.y + 45, 8, UI_GRAY_BLACK, fb);
        }
        ui_nav_draw(fb, 3);
        return;
    }
    if (s_page == SETTINGS_LOCK_STYLE) {
        back_header(fb, "锁屏样式");
        section(fb, 248, "选择电源键锁屏后的画面");
        const char *labels[] = {"壁纸锁屏", "阅读票根"};
        const char *details[] = {"使用 TF 卡中的 JPG / PNG 图片", "书封、进度和阅读记录"};
        const uint8_t values[] = {1, 0};
        for (int i = 0; i < 2; ++i) {
            EpdRect box = {36, 300 + i * 154, 612, 132};
            bool active = app_settings_lock_style() == values[i];
            settings_card(fb, box, 22, active ? 0xd0 : UI_GRAY_WHITE, 0x70);
            ui_text(fb, 64, box.y + 23, 31, labels[i], EPD_DRAW_ALIGN_LEFT, false);
            ui_text(fb, 64, box.y + 79, 20, details[i], EPD_DRAW_ALIGN_LEFT, false);
            if (active) epd_fill_circle(609, box.y + 66, 8, UI_GRAY_BLACK, fb);
        }
        ui_text(fb, 54, 659, 22, "选择壁纸后，可继续从 TF 卡更换图片", EPD_DRAW_ALIGN_LEFT, false);
        if (app_settings_wallpaper_path()[0])
            row(fb, 710, "当前壁纸", strrchr(app_settings_wallpaper_path(), '/') + 1);
        ui_nav_draw(fb, 3);
        return;
    }
    if (s_page == SETTINGS_WALLPAPER) {
        back_header(fb, "选择壁纸");
        section(fb, 244, "TF 卡图片 · JPG / PNG · 不超过 2 MB");
        if (!s_wallpaper_count)
            ui_text(fb, 54, 325, 25, "未找到图片，请放入 TF 卡的 pictures 文件夹", EPD_DRAW_ALIGN_LEFT, false);
        for (int i = 0; i < 8; ++i) {
            int index = s_wallpaper_page * 8 + i;
            if (index >= s_wallpaper_count) break;
            EpdRect box = {36, 292 + i * 92, 612, 78};
            bool active = !strcmp(s_wallpapers[index].path, app_settings_wallpaper_path());
            settings_card(fb, box, 18, active ? 0xd0 : UI_GRAY_WHITE, 0x70);
            char name[96]; snprintf(name, sizeof(name), "%s", s_wallpapers[index].name);
            fit_value(name, 490);
            ui_text_vc(fb, 60, box.y + 39, 24, name, EPD_DRAW_ALIGN_LEFT, false);
            if (active) epd_fill_circle(609, box.y + 39, 6, UI_GRAY_BLACK, fb);
        }
        ui_nav_draw(fb, 3);
        return;
    }
    if (s_page == SETTINGS_WALLPAPER_PREVIEW) {
        back_header(fb, "预览壁纸");
        EpdRect image = {166, 194, 352, 626};
        ui_fill_round_rect(fb, image, 8, UI_GRAY_WHITE);
        s_wallpaper_preview_ok = s_wallpaper_selected >= 0 && s_wallpaper_selected < s_wallpaper_count &&
            ui_wallpaper_draw(fb, s_wallpapers[s_wallpaper_selected].path, image);
        ui_draw_round_rect(fb, image, 8, 0x70);
        if (!s_wallpaper_preview_ok)
            ui_text_vc(fb, 342, 507, 23, "图片无法预览", EPD_DRAW_ALIGN_CENTER, false);
        if (s_wallpaper_selected >= 0 && s_wallpaper_selected < s_wallpaper_count) {
            char name[96]; snprintf(name, sizeof(name), "%s", s_wallpapers[s_wallpaper_selected].name);
            fit_value(name, 570);
            ui_text_vc(fb, 342, 859, 24, name, EPD_DRAW_ALIGN_CENTER, false);
            char position[40];
            snprintf(position, sizeof(position), "%d / %d · 上下滑动切换", s_wallpaper_selected + 1, s_wallpaper_count);
            ui_text_vc(fb, 342, 908, 20, position, EPD_DRAW_ALIGN_CENTER, false);
        }
        if (s_wallpaper_preview_ok) {
            EpdRect button = {94, 962, 496, 70};
            ui_fill_round_rect(fb, button, 22, 0x30);
            ui_text_vc(fb, 342, 997, 25, "设为锁屏壁纸", EPD_DRAW_ALIGN_CENTER, true);
        }
        if (s_wallpaper_confirm) {
            EpdRect dialog = {68, 435, 548, 306};
            ui_fill_round_rect(fb, dialog, 26, UI_GRAY_WHITE);
            ui_draw_round_rect(fb, dialog, 26, 0x50);
            ui_text_vc(fb, 342, 503, 31, "设为壁纸锁屏？", EPD_DRAW_ALIGN_CENTER, false);
            ui_text_vc(fb, 342, 565, 20, "确认后，短按电源键将显示此图片", EPD_DRAW_ALIGN_CENTER, false);
            EpdRect cancel = {94, 643, 232, 68}, confirm = {358, 643, 232, 68};
            ui_fill_round_rect(fb, cancel, 18, 0xe0);
            ui_fill_round_rect(fb, confirm, 18, 0x30);
            ui_text_vc(fb, 210, 677, 24, "取消", EPD_DRAW_ALIGN_CENTER, false);
            ui_text_vc(fb, 474, 677, 24, "确认", EPD_DRAW_ALIGN_CENTER, true);
        }
        ui_nav_draw(fb, 3);
        return;
    }
    if (s_page == SETTINGS_TIME) {
        back_header(fb, "日期与时间");
        time_t now = time(NULL);
        char shown[64] = "未设置";
        if (now >= 1704067200) {
            struct tm tm; time_t local = now + 8 * 3600;
            if (gmtime_r(&local, &tm)) snprintf(shown, sizeof(shown), "%04d-%02d-%02d  %02d:%02d",
                tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min);
        }
        section(fb, 253, "当前时间 · 北京时间");
        row(fb, 296, shown, "");
        row(fb, 407, "通过 WiFi 对时", "立即同步  ›");
        row(fb, 518, "手动设置", "编辑  ›");
        ui_text(fb, 54, 656, 21, "对时成功后由内部时钟持续走时；无需保持联网。", EPD_DRAW_ALIGN_LEFT, false);
        ui_text(fb, 54, 694, 21, "长时间离线可能产生误差，建议定期重新对时。", EPD_DRAW_ALIGN_LEFT, false);
        if (s_notice[0]) ui_text(fb, 54, 772, 22, s_notice, EPD_DRAW_ALIGN_LEFT, false);
        ui_nav_draw(fb, 3);
        return;
    }
    if (s_page == SETTINGS_POWER_SLEEP) {
        back_header(fb, "关机睡眠");
        section(fb, 248, "选择电源菜单中“关机”的方式");
        const char *labels[] = {"彻底关机", "先浅睡，10 分钟后深睡"};
        const char *details[] = {"完全断电，长按电源键开机",
                                 "锁屏画面保留；深睡后短按电源键开机"};
        for (int i = 0; i < 2; ++i) {
            EpdRect box = {36, 300 + i * 154, 612, 132};
            bool active = app_settings_staged_shutdown() == (i == 1);
            settings_card(fb, box, 22, active ? 0xd0 : UI_GRAY_WHITE, 0x70);
            ui_text(fb, 64, box.y + 23, 29, labels[i], EPD_DRAW_ALIGN_LEFT, false);
            ui_text(fb, 64, box.y + 79, 20, details[i], EPD_DRAW_ALIGN_LEFT, false);
            if (active) epd_fill_circle(609, box.y + 66, 8, UI_GRAY_BLACK, fb);
        }
        ui_text(fb, 54, 664, 21, "浅睡时按键直接返回，深睡时会重新开机。", EPD_DRAW_ALIGN_LEFT, false);
        ui_text(fb, 54, 708, 21, "锁屏样式同时适用于壁纸与阅读票根。", EPD_DRAW_ALIGN_LEFT, false);
        ui_nav_draw(fb, 3);
        return;
    }
    if (s_page == SETTINGS_BOOT) {
        back_header(fb, "电脑刷机");
        settings_card(fb, (EpdRect){36, 254, 612, 298}, 22, UI_GRAY_WHITE, 0x70);
        ui_text(fb, 60, 288, 29, "进入 BOOT 模式", EPD_DRAW_ALIGN_LEFT, false);
        ui_text(fb, 60, 350, 22, "用于官网完整刷机或故障恢复", EPD_DRAW_ALIGN_LEFT, false);
        ui_draw_button(fb, (EpdRect){60, 438, 564, 78}, s_boot_pending ? "正在进入" : "进入 BOOT 模式", false);
        ui_text(fb, 48, 620, 22, "请用 USB 连接电脑，再打开官网刷机页", EPD_DRAW_ALIGN_LEFT, false);
        ui_nav_draw(fb, 3); return;
    }

    if (s_page == SETTINGS_CONFIG) {
        back_header(fb, "保存与恢复配置");
        section(fb, 248, "换机或刷机后，快速恢复个性化设置");
        settings_card(fb, (EpdRect){36, 299, 612, 197}, 22, UI_GRAY_WHITE, 0x70);
        ui_text(fb, 60, 326, 26, "TF 卡根目录", EPD_DRAW_ALIGN_LEFT, false);
        ui_text(fb, 60, 377, 23, "Pico-settings.backup", EPD_DRAW_ALIGN_LEFT, false);
        ui_text(fb, 60, 435, 19, "文字排版、设备资料、阅读记录与书签", EPD_DRAW_ALIGN_LEFT, false);
        ui_draw_button(fb, (EpdRect){36, 561, 612, 83}, "保存当前配置到 TF 卡", false);
        ui_draw_button(fb, (EpdRect){36, 681, 612, 83},
                       s_config_confirm ? "再次点按，确认恢复配置" : "从 TF 卡恢复配置", false);
        if (s_notice[0]) ui_text(fb, 48, 819, 21, s_notice, EPD_DRAW_ALIGN_LEFT, false);
        ui_text(fb, 48, 899, 19, "包含阅读记录、设备资料、WiFi和坚果云。", EPD_DRAW_ALIGN_LEFT, false);
        ui_text(fb, 48, 939, 19, "书籍、字体和图片仍需留在 TF 卡。", EPD_DRAW_ALIGN_LEFT, false);
        ui_text(fb, 48, 979, 19, "备份含WiFi和坚果云应用密码，请妥善保管。", EPD_DRAW_ALIGN_LEFT, false);
        ui_nav_draw(fb, 3);
        return;
    }

    // 系统信息保留原106px高度，不再提供头像/设备名入口或卡片边框。
    // Preserve the original 106px information area, without profile editors or a card border.
    const int info_y = 84 - s_main_scroll;
    if (info_y + 106 > 80) {
        int title_px=ui_text_effective_px(32);if(title_px>40)title_px=40;
        ui_text_fixed_vc(fb,36,info_y+36,title_px,"Pico Reader if Edition",EPD_DRAW_ALIGN_LEFT,false);
        ui_text_fixed_vc(fb,36,info_y+78,23,"based on Pico Reader 0.3.3-rc83",EPD_DRAW_ALIGN_LEFT,false);
    }
    char wifi_ssid[33] = {0}; bool wifi_saved = false;
    (void)read_pico_transfer_get_saved_wifi(wifi_ssid, &wifi_saved);
    const char *wireless_labels[] = {"WiFi", "坚果云同步"};
    cloud_config_t cloud; cloud_config_load(&cloud);
    const bool cloud_ready = cloud_config_valid(&cloud, true);
    memset(cloud.password, 0, sizeof(cloud.password));
    char wireless_value[56]; snprintf(wireless_value, sizeof(wireless_value), "%s  ›", wifi_saved ? wifi_ssid : "未配置");
    fit_value(wireless_value, 235);
    const char *wireless_values[] = {wireless_value, cloud_ready ? "已配置  ›" : "未配置  ›"};
    static const int wireless_icons[] = {0, 12};
    setting_group(fb, SETTINGS_WIRELESS_Y - 34, "无线连接", SETTINGS_WIRELESS_Y,
                  wireless_icons, wireless_labels, wireless_values, SETTINGS_WIRELESS_ROWS);
    const char *reading_labels[] = {"系统字体", "系统字号", "系统对比度", "状态栏签名", "首页强刷"};
    char font[96];
    const char *chosen_font = app_settings_system_font_path();
    snprintf(font, sizeof(font), "%s  ›", system_font_label(chosen_font));
    fit_value(font, 235);
    char size[32]; snprintf(size, sizeof(size), "%u%%  ›", app_settings_system_font_size());
    char contrast[32]; snprintf(contrast, sizeof(contrast), "%u%%  ›", app_settings_system_contrast());
    char signature_value[96];
    snprintf(signature_value, sizeof(signature_value), "%s  ›",
             app_settings_status_signature()[0] ? app_settings_status_signature() : "未设置");
    fit_value(signature_value, 235);
    const char *reading_values[] = {font, size, contrast,
                                    signature_value, app_settings_home_full_refresh() ? "开启  ›" : "关闭  ›"};
    static const int reading_icons[] = {2, 3, 7, 10, 11};
    setting_group(fb, SETTINGS_DISPLAY_Y - 34, "显示", SETTINGS_DISPLAY_Y,
                  reading_icons, reading_labels, reading_values, SETTINGS_DISPLAY_ROWS);
    const char *display_labels[] = {"锁屏样式", "关机睡眠", "日期与时间"};
    const char *display_values[] = {app_settings_lock_style() ? "壁纸  ›" : "阅读票根  ›",
                                    app_settings_staged_shutdown() ? "先浅后深  ›" : "彻底断电  ›", "设置  ›"};
    static const int display_icons[] = {5, 9, 6};
    setting_group(fb, SETTINGS_DEVICE_Y - 34, "阅读与设备", SETTINGS_DEVICE_Y,
                  display_icons, display_labels, display_values, SETTINGS_DEVICE_ROWS);
    static const char *const maintenance_labels[] = {"系统升级", "保存与恢复", "BOOT 刷机"};
    static const char *const maintenance_values[] = {"TF 卡  ›", "配置  ›", "电脑刷机  ›"};
    static const int maintenance_icons[] = {11, 8, 11};
    setting_group(fb, SETTINGS_MAINTENANCE_Y - 34, "升级和恢复", SETTINGS_MAINTENANCE_Y,
                  maintenance_icons, maintenance_labels, maintenance_values, SETTINGS_MAINTENANCE_ROWS);
    epd_fill_rect((EpdRect){0, 0, UI_LOCK_WIDTH, 80}, 0xe0, fb);
    ui_nav_status(fb);
    ui_nav_draw(fb, 3);
}

static void on_enter(app_ctx_t *ctx) {
    (void)ctx;
    quote_preview_clear();s_quote_px=content_home_quote_size();s_quote_dragging=false;s_quote_committed=false;
    s_notice[0] = 0;
    s_page = s_cloud_requested?SETTINGS_CLOUD:SETTINGS_MAIN;s_cloud_requested=false;s_cloud_editor=0;
    s_main_scroll = s_font_page = s_wallpaper_page = 0;
    s_scroll_drag_consumed = s_scroll_present_pending = false;
    s_wallpaper_selected = -1;
    s_wallpaper_confirm = s_wallpaper_preview_ok = false;
    s_sync_pending = s_boot_pending = false;
    s_config_confirm = false;
    s_boot_pending = s_local_pending = s_upgrade_restart = false;
    s_upgrade_confirm = false;
    s_home_quote_editor=false;
    if(s_home_quote_requested){s_home_quote_requested=false;s_page=SETTINGS_HOME_QUOTE;quote_preview_prepare(s_quote_px,false);}
}

static app_redraw_t on_tick(app_ctx_t *ctx) {
    if(s_page==SETTINGS_HOME_QUOTE&&!s_quote_preview&&!s_quote_preview_attempted){quote_preview_prepare(content_home_quote_size(),false);return APP_REDRAW_PAGE;}
    (void)ctx;
    if (s_boot_pending) {
        s_boot_pending = false;
        pmu_selftest_prepare_powerdown();
        uint8_t req[2] = {0, 0};
        esp_err_t error = read_pico_pmu_cmd(PMU_CMD_HOST_REQUEST_RESET, req, sizeof(req));
        if (error != ESP_OK) ESP_LOGW(TAG, "BOOT PMU notice: %s", esp_err_to_name(error));
        REG_WRITE(RTC_CNTL_OPTION1_REG, RTC_CNTL_FORCE_DOWNLOAD_BOOT);
        esp_restart(); return APP_REDRAW_NONE;
    }
    if (s_upgrade_restart) { s_upgrade_restart = false; esp_restart(); return APP_REDRAW_NONE; }
    if (s_page == SETTINGS_UPGRADE) {
        if (s_local_pending) {
            s_local_pending = false;
            esp_err_t error = pico_ota_install(PICO_OTA_UPDATE_PATH, s_upgrade_notice, sizeof(s_upgrade_notice));
            s_upgrade_restart = error == ESP_OK;
            return APP_REDRAW_PAGE;
        }
        return APP_REDRAW_NONE;
    }
    if (!s_sync_pending) return APP_REDRAW_NONE;
    s_sync_pending = false;
    uint32_t utc = 0;
    read_pico_transfer_status_t network = {0};
    read_pico_transfer_get_status(&network);
    esp_err_t err = read_pico_transfer_sync_time(&utc);
    bool network_time_received = err == ESP_OK;
    if (network_time_received) {
        uint8_t payload[4] = {(uint8_t)utc, (uint8_t)(utc >> 8), (uint8_t)(utc >> 16), (uint8_t)(utc >> 24)};
        err = read_pico_pmu_cmd(PMU_CMD_TIME_SYNC, payload, sizeof(payload));
        if (err == ESP_OK) err = read_pico_pmu_cmd(PMU_CMD_TIME_GET, NULL, 0);
        if (err == ESP_OK) {
            const pmu_snapshot_t *pmu = read_pico_pmu_get();
            if (!pmu || !pmu->time_synced || pmu->unix_sec < utc || pmu->unix_sec - utc > 10)
                err = ESP_ERR_INVALID_RESPONSE;
        }
    }
    ESP_LOGI(TAG, "time sync network=%d ready=%d utc=%lu result=%s",
             network.mode, network.network_ready, (unsigned long)utc, esp_err_to_name(err));
    const char *notice = err == ESP_OK ? "WiFi 对时完成，内部时钟已接管" :
        network_time_received ? "已获取网络时间，内部时钟写入失败" :
        err == ESP_ERR_NOT_FOUND ? "请先在 WiFi 设置中配置网络" :
        err == ESP_ERR_INVALID_STATE ? "WiFi 尚未连接，请稍后再试" :
        err == ESP_ERR_TIMEOUT ? "网络对时超时，请检查网络后重试" :
        "网络对时失败，请检查网络后重试";
    snprintf(s_notice, sizeof(s_notice), "%s", notice);
    return APP_REDRAW_PAGE;
}

static app_redraw_t profile_editor_gesture(app_ctx_t *ctx,const ui_gesture_event_t *ev) {
    ui_input_result_t result=ui_text_input_gesture(&s_text_input,ev);
    if(result==UI_INPUT_NONE)return APP_REDRAW_NONE;
    if(result==UI_INPUT_CANCEL){ui_text_input_clear(&s_text_input);if(s_home_quote_editor){s_home_quote_editor=false;s_page=SETTINGS_HOME_QUOTE;quote_preview_prepare(content_home_quote_size(),false);return APP_REDRAW_PAGE;}s_page=editor_return_page();return APP_REDRAW_PAGE;}
    if(result==UI_INPUT_CHANGED){s_editor_notice[0]=0;return APP_REDRAW_AREA;}
    if(s_home_quote_editor){
        if(!ui_banner_quote_fits_font(s_editor,content_home_quote_font())){strlcpy(s_editor_notice,"文字最多显示两行，请缩短后保存",sizeof(s_editor_notice));return APP_REDRAW_PAGE;}
        if(content_home_quote_set(s_editor)!=ESP_OK){strlcpy(s_editor_notice,"保存失败，请检查设置存储空间",sizeof(s_editor_notice));return APP_REDRAW_PAGE;}
        ui_text_input_clear(&s_text_input);s_home_quote_editor=false;s_quote_committed=true;s_page=SETTINGS_HOME_QUOTE;quote_preview_prepare(content_home_quote_size(),false);return APP_REDRAW_PAGE;
    }
    if(s_cloud_editor){
        cloud_config_t cfg;cloud_config_load(&cfg);
        if(s_cloud_editor==1)strlcpy(cfg.folder,s_editor,sizeof(cfg.folder));
        else if(s_cloud_editor==2){if(strcmp(cfg.account,s_editor))memset(cfg.password,0,sizeof(cfg.password));strlcpy(cfg.account,s_editor,sizeof(cfg.account));}
        else if(s_editor[0])snprintf(cfg.password,sizeof(cfg.password),"%s",s_editor);
        esp_err_t e=cloud_config_save(&cfg);memset(cfg.password,0,sizeof(cfg.password));
        if(e!=ESP_OK){snprintf(s_editor_notice,sizeof(s_editor_notice),"%s",e==ESP_ERR_NOT_SUPPORTED?"模拟器请使用页面下方连接配置":"内容无效或保存失败");return APP_REDRAW_PAGE;}
        memset(s_editor,0,sizeof(s_editor));s_page=SETTINGS_CLOUD;return APP_REDRAW_PAGE;
    }
    app_settings_set_status_signature(s_editor);
    const char *saved = app_settings_status_signature();
    if (strcmp(saved, s_editor)) {
        snprintf(s_editor_notice, sizeof(s_editor_notice), "保存失败，请检查设置存储空间");
        return APP_REDRAW_PAGE;
    }
    s_page = editor_return_page();
    return APP_REDRAW_PAGE;
}

// 手指越过阈值就推进至少三行；一轮拖动只刷新一次，松手不再重复滚动。
// Advance at least three rows on crossing the drag threshold; refresh once per drag, without repeating on release.
static app_redraw_t scroll_gesture(const ui_gesture_event_t *ev, int *offset, int limit, bool *handled) {
    *handled = ev->type != UI_GESTURE_TAP || s_scroll_drag_consumed;
    if (ev->type == UI_GESTURE_PRESS) {
        s_scroll_drag_consumed = false;
        return APP_REDRAW_NONE;
    }
    if (ev->type == UI_GESTURE_TAP || ev->type == UI_GESTURE_CANCEL) {
        s_scroll_drag_consumed = false;
        return APP_REDRAW_NONE;
    }
    if (ev->type != UI_GESTURE_MOVE && ev->type != UI_GESTURE_SWIPE_U && ev->type != UI_GESTURE_SWIPE_D)
        return APP_REDRAW_NONE;
    if (ev->y0 < (s_page == SETTINGS_MAIN ? 80 : 190) || ev->y0 >= UI_NAV_TOP)
        return APP_REDRAW_NONE;
    if (s_scroll_drag_consumed) {
        if (ev->type != UI_GESTURE_MOVE) s_scroll_drag_consumed = false;
        return APP_REDRAW_NONE;
    }
    int dy = (int)ev->y - ev->y0, dx = (int)ev->x - ev->x0;
    int distance = abs(dy);
    if (ev->type == UI_GESTURE_MOVE && (distance < 108 || distance <= abs(dx))) return APP_REDRAW_NONE;
    int step = distance < 204 ? 204 : distance > 450 ? 450 : distance;
    bool up = ev->type == UI_GESTURE_SWIPE_U || (ev->type == UI_GESTURE_MOVE && dy < 0);
    int next = *offset + (up ? step : -step);
    if (limit < 0) limit = 0;
    if (next < 0) next = 0;
    if (next > limit) next = limit;
    if (ev->type == UI_GESTURE_MOVE) s_scroll_drag_consumed = true;
    if (next == *offset) return APP_REDRAW_NONE;
    *offset = next;
    s_scroll_present_pending = true;
    return APP_REDRAW_AREA;
}

static app_redraw_t on_gesture(app_ctx_t *ctx, const ui_gesture_event_t *ev) {
    if (s_page == SETTINGS_BOOT) {
        if (ev->type != UI_GESTURE_TAP || s_boot_pending) return APP_REDRAW_NONE;
        if (ev->y0 < 190 && ev->x0 < 170) { s_page = SETTINGS_MAIN; return APP_REDRAW_PAGE; }
        if (ui_rect_hit((EpdRect){60, 438, 564, 78}, ev->x0, ev->y0)) { s_boot_pending = true; return APP_REDRAW_PAGE; }
        int tab = ui_nav_hit(ev->x0, ev->y0);
        if (tab >= 0) ui_nav_request(ctx, tab);
        return APP_REDRAW_NONE;
    }
    if (s_page == SETTINGS_UPGRADE) {
        if (ev->type != UI_GESTURE_TAP || s_local_pending || s_upgrade_restart) return APP_REDRAW_NONE;
        int y = ev->y0;
        if (y < 190 && ev->x0 < 170) {
            if (s_upgrade_confirm) s_upgrade_confirm = false; else s_page = SETTINGS_MAIN;
            return APP_REDRAW_PAGE;
        }
        int tab = ui_nav_hit(ev->x0, y);
        if (tab >= 0) { s_upgrade_confirm = false; ui_nav_request(ctx, tab); return APP_REDRAW_NONE; }
        if (y >= 613 && y < 677 && ev->x0 >= 60 && ev->x0 < 624) {
            if (s_upgrade_confirm) { s_upgrade_confirm = false; if (ev->x0 >= 364) s_local_pending = true; }
            else { (void)pico_ota_inspect(PICO_OTA_UPDATE_PATH, &s_local_update); s_upgrade_confirm = s_local_update.ready; if (!s_local_update.ready) snprintf(s_upgrade_notice,sizeof(s_upgrade_notice),"%s",s_local_update.message); }
            return APP_REDRAW_PAGE;
        }
        return APP_REDRAW_NONE;
    }
    if(s_page==SETTINGS_HOME_QUOTE){
        bool handled;app_redraw_t redraw=quote_slider_gesture(ev,&handled);if(handled)return redraw;
        if(ev->type!=UI_GESTURE_TAP)return APP_REDRAW_NONE;
        if(ev->y0<190||ui_rect_hit((EpdRect){36,1064,612,56},ev->x0,ev->y0)){ui_nav_request(ctx,0);return APP_REDRAW_NONE;}
        if(ui_rect_hit((EpdRect){36,572,612,72},ev->x0,ev->y0)){home_quote_editor_open();return APP_REDRAW_PAGE;}
        if(ui_rect_hit((EpdRect){36,662,612,72},ev->x0,ev->y0)){ttf_font_scan();s_font_page=0;s_page=SETTINGS_QUOTE_FONT;s_notice[0]=0;return APP_REDRAW_PAGE;}
        for(int i=0;i<4;++i)if(ui_rect_hit(quote_alignment_rect(i),ev->x0,ev->y0)){
            if(content_home_quote_alignment()==(content_quote_align_t)i)return APP_REDRAW_NONE;
            (void)quote_preview_prepare_alignment(content_home_quote_size(),false,(content_quote_align_t)i,true);return APP_REDRAW_PAGE;
        }
        int tab=ui_nav_hit(ev->x0,ev->y0);if(tab>=0)ui_nav_request(ctx,tab);return APP_REDRAW_NONE;
    }
    if(s_page==SETTINGS_QUOTE_FONT&&ev->type==UI_GESTURE_TAP&&ev->y0<190){s_page=SETTINGS_HOME_QUOTE;return APP_REDRAW_PAGE;}
    if (s_page == SETTINGS_TEXT_EDIT) return profile_editor_gesture(ctx,ev);
    if (s_page == SETTINGS_MAIN) {
        int limit = SETTINGS_SCROLL_MAX;
        int *offset = &s_main_scroll;

        bool handled;
        app_redraw_t redraw = scroll_gesture(ev, offset, limit, &handled);
        if (handled) return redraw;
    }
    if (s_page == SETTINGS_WALLPAPER_PREVIEW && !s_wallpaper_confirm &&
        (ev->type == UI_GESTURE_SWIPE_U || ev->type == UI_GESTURE_SWIPE_D)) {
        int next = s_wallpaper_selected + (ev->type == UI_GESTURE_SWIPE_U ? 1 : -1);
        if (next < 0 || next >= s_wallpaper_count) return APP_REDRAW_NONE;
        s_wallpaper_selected = next;
        s_wallpaper_page = next / 8;
        return APP_REDRAW_PAGE;
    }
    if ((s_page == SETTINGS_SYSTEM_FONT || s_page == SETTINGS_QUOTE_FONT || s_page == SETTINGS_WALLPAPER) &&
        (ev->type == UI_GESTURE_SWIPE_U || ev->type == UI_GESTURE_SWIPE_D)) {
        int *page = (s_page == SETTINGS_SYSTEM_FONT || s_page == SETTINGS_QUOTE_FONT) ? &s_font_page : &s_wallpaper_page;
        int count = (s_page == SETTINGS_SYSTEM_FONT || s_page == SETTINGS_QUOTE_FONT) ? ttf_font_count() + 1 : s_wallpaper_count;
        int pages = count > 0 ? (count + 7) / 8 : 1;
        int old = *page;
        if (ev->type == UI_GESTURE_SWIPE_U && *page + 1 < pages) ++*page;
        if (ev->type == UI_GESTURE_SWIPE_D && *page > 0) --*page;
        if (*page == old) return APP_REDRAW_NONE;
        s_scroll_present_pending = true;
        return APP_REDRAW_AREA;
    }
    if (ev->type != UI_GESTURE_TAP) return APP_REDRAW_NONE;
    if (s_page == SETTINGS_WALLPAPER_PREVIEW && s_wallpaper_confirm) {
        if (ev->y0 >= 643 && ev->y0 < 711 && ev->x0 >= 358 && ev->x0 < 590) {
            const char *path = s_wallpapers[s_wallpaper_selected].path;
            app_settings_set_wallpaper_path(path);
            if (!strcmp(app_settings_wallpaper_path(), path)) {
                app_settings_set_lock_style(1);
                s_page = SETTINGS_LOCK_STYLE;
            }
        }
        s_wallpaper_confirm = false;
        return APP_REDRAW_PAGE;
    }
    if (s_page == SETTINGS_TIME_EDIT) {
        if (ev->y0 < 190) { s_page = SETTINGS_TIME; return APP_REDRAW_PAGE; }
        if (ev->y0 >= 354 && ev->y0 < 597) {
            int field = (ev->x0 - 36) / 122;
            if (ev->x0 >= 36 && field >= 0 && field < 5 && (ev->x0 - 36) % 122 < 106) {
                if (ev->y0 < 416) time_adjust(field, 1);
                else if (ev->y0 >= 535) time_adjust(field, -1);
                else return APP_REDRAW_NONE;
                return APP_REDRAW_PAGE;
            }
        }
        if (ev->y0 >= 767 && ev->y0 < 845) { (void)time_save(); return APP_REDRAW_PAGE; }
        if (ev->y0 >= 874 && ev->y0 < 952) { s_page = SETTINGS_TIME; return APP_REDRAW_PAGE; }
        return APP_REDRAW_NONE;
    }
    int tab = ui_nav_hit(ev->x0, ev->y0);
    if (tab >= 0) { ui_nav_request(ctx, tab); return APP_REDRAW_NONE; }
    int y = ev->y0;
    if (s_page == SETTINGS_WALLPAPER_PREVIEW) {
        if (y < 190) { s_page = SETTINGS_WALLPAPER; return APP_REDRAW_PAGE; }
        if (s_wallpaper_preview_ok && y >= 194 && y < 1032) {
            s_wallpaper_confirm = true;
            return APP_REDRAW_PAGE;
        }
        return APP_REDRAW_NONE;
    }
    if (s_page == SETTINGS_WALLPAPER && y < 190) {
        s_page = SETTINGS_LOCK_STYLE;
        return APP_REDRAW_PAGE;
    }

    if (s_page != SETTINGS_MAIN && y < 190) { s_page = SETTINGS_MAIN; return APP_REDRAW_PAGE; }
    if (s_page == SETTINGS_WIFI) {
        if (y >= 408 && y < 478) {
            extern const app_desc_t app_transfer;
            app_transfer_request_wifi_setup();
            ctx->request_app = &app_transfer;
        }
        return APP_REDRAW_NONE;
    }
    if (s_page == SETTINGS_SYSTEM_FONT || s_page == SETTINGS_QUOTE_FONT) {
        if (y < 292 || y >= 1028) return APP_REDRAW_NONE;
        int index = s_font_page * 8 + (y - 292) / 92;
        int count = ttf_font_count();
        if (index > count) return APP_REDRAW_NONE;
        const ttf_font_item_t *item = index ? ttf_font_item(index - 1) : NULL;
        if(s_page==SETTINGS_QUOTE_FONT){
            if(!ui_banner_quote_fits_font(content_home_quote(),item?item->path:"")){strlcpy(s_notice,"此字体下超出两行，请先缩短文字",sizeof(s_notice));return APP_REDRAW_PAGE;}
            esp_err_t e=content_home_quote_font_set(item?item->path:"");
            if(e!=ESP_OK){strlcpy(s_notice,"字体设置保存失败",sizeof(s_notice));return APP_REDRAW_PAGE;}
            s_quote_committed=true;s_page=SETTINGS_HOME_QUOTE;quote_preview_prepare(content_home_quote_size(),false);return APP_REDRAW_PAGE;
        }
        app_settings_set_system_font_path(item ? item->path : "");
        app_font_activate_system();
        return APP_REDRAW_PAGE;
    }
    if (s_page == SETTINGS_SYSTEM_SIZE) {
        for (int index = 0; index < 11; ++index) {
            EpdRect box = {36 + (index % 2) * 316, 298 + (index / 2) * 122, 296, 102};
            if (!ui_rect_hit(box, ev->x0, y)) continue;
            app_settings_set_system_font_size((uint8_t)(100 + index * 10));
            ui_text_set_system_scale(true);
            return APP_REDRAW_PAGE;
        }
        return APP_REDRAW_NONE;
    }
    if (s_page == SETTINGS_SYSTEM_CONTRAST) {
        if (y < 298 || y >= 878) return APP_REDRAW_NONE;
        int index = (y - 298) / 116;
        app_settings_set_system_contrast((uint8_t)(100 + index * 10));
        return APP_REDRAW_PAGE;
    }
    if (s_page == SETTINGS_LOCK_STYLE) {
        if (y >= 300 && y < 432) {
            wallpaper_scan();
            s_page = SETTINGS_WALLPAPER;
            return APP_REDRAW_PAGE;
        }
        if (y >= 454 && y < 586) {
            app_settings_set_lock_style(0);
            return APP_REDRAW_PAGE;
        }
        if (y >= 710 && app_settings_wallpaper_path()[0]) {
            wallpaper_scan();
            s_page = SETTINGS_WALLPAPER;
            return APP_REDRAW_PAGE;
        }
        return APP_REDRAW_NONE;
    }
    if (s_page == SETTINGS_WALLPAPER) {
        if (y < 292 || y >= 1028) return APP_REDRAW_NONE;
        int index = s_wallpaper_page * 8 + (y - 292) / 92;
        if (index >= s_wallpaper_count) return APP_REDRAW_NONE;
        s_wallpaper_selected = index;
        s_wallpaper_confirm = false;
        s_page = SETTINGS_WALLPAPER_PREVIEW;
        return APP_REDRAW_PAGE;
    }
    if (s_page == SETTINGS_TIME) {
        if (y >= 407 && y < 477) {
            s_notice[0] = 0; s_sync_pending = true;
            snprintf(s_notice, sizeof(s_notice), "正在获取网络时间…");
            return APP_REDRAW_PAGE;
        }
        if (y >= 518 && y < 588) { time_open(); return APP_REDRAW_PAGE; }
        return APP_REDRAW_NONE;
    }
    if(s_page==SETTINGS_CLOUD){
        if(y>=254&&y<338)cloud_editor_open(1);
        else if(y>=354&&y<438)cloud_editor_open(2);
        else if(y>=454&&y<538)cloud_editor_open(3);
        return APP_REDRAW_PAGE;
    }
    if (s_page == SETTINGS_POWER_SLEEP) {
        if (y >= 300 && y < 432) {
            app_settings_set_staged_shutdown(false);
            return APP_REDRAW_PAGE;
        }
        if (y >= 454 && y < 586) {
            app_settings_set_staged_shutdown(true);
            return APP_REDRAW_PAGE;
        }
        return APP_REDRAW_NONE;
    }
    if (s_page == SETTINGS_CONFIG) {
        if (y >= 561 && y < 644) {
            s_config_confirm = false;
            esp_err_t err = app_settings_backup_save();
            snprintf(s_notice, sizeof(s_notice), "%s",
                     err == ESP_OK ? "已保存到 TF 卡根目录" :
                     err == ESP_ERR_INVALID_STATE ? "请检查TF卡、结束同步或先重试恢复" :
                     "保存失败，请检查 TF 卡剩余空间");
            return APP_REDRAW_PAGE;
        }
        if (y >= 681 && y < 764) {
            if (!s_config_confirm) {
                s_config_confirm = true;
                snprintf(s_notice, sizeof(s_notice), "恢复将覆盖当前设置，请再点按一次确认");
                return APP_REDRAW_PAGE;
            }
            s_config_confirm = false;
            esp_err_t err = app_settings_backup_restore();
            if (err == ESP_OK) {
                ttf_font_scan();
                app_font_activate_system();
                ui_text_set_system_scale(true);
                // 阅读记录恢复后令首页和书架重新取进度、时长与排序。
                // Rebuild home and shelf caches after restoring reading records.
                book_store_notify_changed();
            }
            char saved_ssid[33] = {0};
            bool wifi_saved = false;
            if (err == ESP_OK)
                (void)read_pico_transfer_get_saved_wifi(saved_ssid, &wifi_saved);
            snprintf(s_notice, sizeof(s_notice), "%s",
                     err == ESP_OK ? (wifi_saved ? "配置与阅读记录已恢复；WiFi 请重新连接" :
                                                   "配置与阅读记录已恢复") :
                     err == ESP_ERR_INVALID_STATE ? "请检查TF卡，并先结束坚果云同步" :
                     err == ESP_ERR_NOT_FOUND ? "未找到 Pico-settings.backup" :
                     err == ESP_ERR_INVALID_RESPONSE ? "配置文件损坏或版本不兼容" :
                     "恢复未完成，请重试");
            return APP_REDRAW_PAGE;
        }
        return APP_REDRAW_NONE;
    }

    if (y < 80 || y >= UI_NAV_TOP) return APP_REDRAW_NONE;
    y += s_main_scroll;
    if (y >= SETTINGS_WIRELESS_Y && y < SETTINGS_WIRELESS_Y + SETTINGS_ROW_H) {
        extern const app_desc_t app_transfer;
        app_transfer_request_wifi_setup();
        ctx->request_app = &app_transfer;
        return APP_REDRAW_NONE;
    }
    if (y >= SETTINGS_WIRELESS_Y + SETTINGS_ROW_H && y < SETTINGS_WIRELESS_Y + 2 * SETTINGS_ROW_H) {
        s_page = SETTINGS_CLOUD; s_notice[0] = 0; return APP_REDRAW_PAGE;
    }

    if (y >= SETTINGS_DISPLAY_Y && y < SETTINGS_DISPLAY_Y + SETTINGS_ROW_H) {
        ttf_font_scan();
        s_font_page = 0;
        s_page = SETTINGS_SYSTEM_FONT;
        return APP_REDRAW_PAGE;
    }
    if (y >= SETTINGS_DISPLAY_Y + SETTINGS_ROW_H && y < SETTINGS_DISPLAY_Y + 2 * SETTINGS_ROW_H) {
        s_page = SETTINGS_SYSTEM_SIZE;
        return APP_REDRAW_PAGE;
    }
    if (y >= SETTINGS_DISPLAY_Y + 2 * SETTINGS_ROW_H && y < SETTINGS_DISPLAY_Y + 3 * SETTINGS_ROW_H) {
        s_page = SETTINGS_SYSTEM_CONTRAST;
        return APP_REDRAW_PAGE;
    }
    if(y>=SETTINGS_DISPLAY_Y+3*SETTINGS_ROW_H&&y<SETTINGS_DISPLAY_Y+4*SETTINGS_ROW_H){profile_editor_open(true);return APP_REDRAW_PAGE;}
    if(y>=SETTINGS_DISPLAY_Y+4*SETTINGS_ROW_H&&y<SETTINGS_DISPLAY_Y+5*SETTINGS_ROW_H){app_settings_set_home_full_refresh(!app_settings_home_full_refresh());return APP_REDRAW_PAGE;}
    if (y >= SETTINGS_DEVICE_Y && y < SETTINGS_DEVICE_Y + SETTINGS_ROW_H) {
        s_page = SETTINGS_LOCK_STYLE;
        return APP_REDRAW_PAGE;
    }
    if (y >= SETTINGS_DEVICE_Y + SETTINGS_ROW_H && y < SETTINGS_DEVICE_Y + 2 * SETTINGS_ROW_H) {
        s_page = SETTINGS_POWER_SLEEP;
        return APP_REDRAW_PAGE;
    }
    if (y >= SETTINGS_DEVICE_Y + 2 * SETTINGS_ROW_H && y < SETTINGS_DEVICE_Y + 3 * SETTINGS_ROW_H) {
        s_page = SETTINGS_TIME;
        return APP_REDRAW_PAGE;
    }
    if (y >= SETTINGS_MAINTENANCE_Y && y < SETTINGS_MAINTENANCE_Y + SETTINGS_ROW_H) {
        upgrade_open(); return APP_REDRAW_PAGE;
    }
    if (y >= SETTINGS_MAINTENANCE_Y + SETTINGS_ROW_H && y < SETTINGS_MAINTENANCE_Y + 2 * SETTINGS_ROW_H) {
        s_page = SETTINGS_CONFIG; s_config_confirm = false; s_notice[0] = 0; return APP_REDRAW_PAGE;
    }
    if (y >= SETTINGS_MAINTENANCE_Y + 2 * SETTINGS_ROW_H && y < SETTINGS_MAINTENANCE_Y + 3 * SETTINGS_ROW_H) {
        s_boot_pending = false; s_page = SETTINGS_BOOT; return APP_REDRAW_PAGE;
    }

    return APP_REDRAW_NONE;
}
static app_redraw_t on_key(app_ctx_t *ctx, int key) {
    if (s_page == SETTINGS_UPGRADE) {
        if (s_local_pending || s_upgrade_restart) return APP_REDRAW_NONE;
        s_upgrade_confirm = false;
    }

    if (key == 1) { ui_nav_request(ctx, 0); return APP_REDRAW_NONE; }
    if (s_page == SETTINGS_WALLPAPER_PREVIEW) {
        if (s_wallpaper_confirm) s_wallpaper_confirm = false;
        else s_page = SETTINGS_WALLPAPER;
        return APP_REDRAW_PAGE;
    }
    if (s_page == SETTINGS_WALLPAPER) { s_page = SETTINGS_LOCK_STYLE; return APP_REDRAW_PAGE; }
    if(s_page==SETTINGS_HOME_QUOTE){ui_nav_request(ctx,0);return APP_REDRAW_NONE;}
    if(s_page==SETTINGS_QUOTE_FONT){s_page=SETTINGS_HOME_QUOTE;return APP_REDRAW_PAGE;}
    if (s_page == SETTINGS_TEXT_EDIT) { memset(s_editor,0,sizeof(s_editor));if(s_home_quote_editor){s_home_quote_editor=false;s_page=SETTINGS_HOME_QUOTE;quote_preview_prepare(content_home_quote_size(),false);return APP_REDRAW_PAGE;}s_page = editor_return_page(); return APP_REDRAW_PAGE; }
    if (s_page == SETTINGS_TIME_EDIT) { s_page = SETTINGS_TIME; return APP_REDRAW_PAGE; }
    if (s_page != SETTINGS_MAIN) { s_page = SETTINGS_MAIN; return APP_REDRAW_PAGE; }
    ui_nav_request(ctx, 0);
    return APP_REDRAW_NONE;
}
static void settings_exit(app_ctx_t *ctx) {
    (void)ctx; quote_preview_clear();s_quote_dragging=false;ui_text_input_clear(&s_text_input);memset(s_editor,0,sizeof(s_editor));
}
static void on_before_lock(app_ctx_t *ctx) {
    (void)ctx; s_quote_dragging=false;
    // 锁屏保留普通草稿；密码清除内容、光标及显示状态，唤醒后可重新输入。
    // Keep ordinary drafts across lock; wipe secret text, cursor and reveal state together.
    if (s_page == SETTINGS_TEXT_EDIT && s_text_input.secret) ui_text_input_clear(&s_text_input);
}
static bool no_menu_handle(app_ctx_t *ctx) { (void)ctx; return false; }
static bool settings_present(app_ctx_t *ctx, app_redraw_t redraw) {
    if(redraw==APP_REDRAW_AREA&&s_page==SETTINGS_HOME_QUOTE){render(ctx,ctx->fb);guard_draw_result(ctx->hl,update_display_area_diff_with(ctx->hl,&E0470_WAVEFORM,MODE_GL16,(EpdRect){0,752,UI_LOCK_WIDTH,182}));return true;}
    if(redraw==APP_REDRAW_AREA && s_page==SETTINGS_TEXT_EDIT){
        render(ctx,ctx->fb);
        guard_draw_result(ctx->hl,update_display_area_diff_with(ctx->hl,&E0470_WAVEFORM,MODE_GL16,ui_text_input_area()));
        return true;
    }
    if (redraw == APP_REDRAW_AREA && s_scroll_present_pending &&
        (s_page == SETTINGS_MAIN ||
         s_page == SETTINGS_SYSTEM_FONT ||
         s_page == SETTINGS_WALLPAPER)) {
        s_scroll_present_pending = false;
        render(ctx, ctx->fb);
        int top = s_page == SETTINGS_MAIN ? 80 : 190;
        EpdRect area = {0, top, UI_LOCK_WIDTH, UI_NAV_TOP - top};
        // 只驱动变化像素，滚动次数不触发黑白清屏；题头与底栏保持稳定。
        // Drive changed pixels only; scroll counts never trigger a wipe, and header/nav stay stable.
        guard_draw_result(ctx->hl, update_display_area_diff_with(ctx->hl, &E0470_WAVEFORM, MODE_GL16, area));
        return true;
    }
    return false;
}

const app_desc_t app_device_settings = {
    .title = "设置 Settings", .detail = "显示、连接与设备", .enter_full = false,
    .owns_keys = true, .menu_handle_enabled = no_menu_handle,
    .on_enter = on_enter, .on_exit = settings_exit, .on_before_lock = on_before_lock, .render = render, .on_tick = on_tick, .on_gesture = on_gesture, .on_key = on_key,
    .present = settings_present,
};
