/* SPDX-License-Identifier: Apache-2.0
 * 中文：复用 rc58 的真实书源、书架、阅读绘制及手势，仅操作虚拟存储。
 * English: rc58 book sources, shelf, rendering and gestures over virtual storage.
 */
#include "settings.h"
#define app_book simulator_descriptor_book
#include "../../main/apps/app_book.c"
#include "replay.h"
extern int simulator_route,simulator_book_view,simulator_reader_panel,simulator_reader_page,simulator_reader_px;
extern bool simulator_reader_fullscreen,simulator_toc_jump;
unsigned simulator_reader_progress(void){return percent(s_page);}
bool simulator_document_image_ready(void){return book_kind()==BOOK_KIND_MARKDOWN&&s_inline_gray&&book_layout_page_image(s_page)==s_inline_index;}
bool simulator_document_zoom(void){return s_document_zoom;}
unsigned simulator_document_image_top(void){return s_zoom_top;}
size_t simulator_document_count(void){return s_document_count;}
bool simulator_document_starred_only(void){return s_document_starred_only;}
bool simulator_document_menu(void){return s_document_menu;}
bool simulator_document_delete_confirm(void){return s_document_delete_confirm;}
const char *simulator_document_first(void){return s_document_count?s_documents[0].path:"";}
const char *simulator_document_item(unsigned i){return i<s_document_count?s_documents[i].path:"";}
const char *simulator_shelf_item(unsigned i){return i<(unsigned)s_visible_count?s_shelf[i].path:"";}
bool simulator_content_sort_open(void){return s_sort_open;}
unsigned simulator_document_selected_count(void){return document_selected_count();}
bool simulator_document_batch_confirm(void){return s_document_batch_confirm;}
unsigned simulator_content_sort_mode(bool documents){return documents?s_document_options.mode:s_book_options.mode;}
const char *simulator_book_path(void){return s_requested_open[0]?s_requested_open:s_path;}
size_t simulator_book(uint8_t *fb,int view,int panel,int page,int px,const char *path,const char *events){
 app_ctx_t ctx={.fb=fb,.now_ms=1000};on_enter(&ctx);scan_shelf(&ctx);scan_documents(&ctx);s_scan_pending=false;
 ctx.leaf=(view!=READING&&view!=TOC)?page:0;
 if(view==READING||view==TOC){
   if(!open_book(&ctx,path))return 0;
   // -1是跨页面打开的续读请求；按原文字节位置重排，不用界面的默认第0页覆盖它。
   // -1 requests resume across app routes; reflow the saved text offset instead of overriding it with UI page zero.
   size_t resume_offset=reader_page_offset(s_page);
   s_reader_fullscreen=panel==100;if(book_kind()!=BOOK_KIND_MARKDOWN)s_px=px;
   if(!load_chapter(&ctx,s_chapter,page<0?resume_offset:0,false))return 0;
   if(page>=0)s_page=(size_t)page;
   if(s_page>=book_layout_page_count())s_page=book_layout_page_count()-1;prepare_inline_image();
 }
 if(panel==READER_PANEL_FONT_PICKER)ttf_font_scan();
 s_view=(book_view_t)view;s_reader_panel=view==READING?(reader_panel_t)panel:READER_PANEL_NONE;
 if(view==DOCUMENTS||view==DOCUMENT_BULK)s_document_mode=true;
 s_reader_fullscreen=panel==100;if(panel==100)s_reader_panel=READER_PANEL_NONE;
 if(view==TOC){s_toc_jump_open=panel==1;s_toc_jump_percent=45;}
 if(s_count){s_managed=s_shelf[0];snprintf(s_editor_title,sizeof(s_editor_title),"%s",s_managed.name);}
 if(view==EDIT)editor_start();
 if(view==SEARCH){s_search_parent=BULK;ui_text_input_open(&s_search_input,s_search_draft,sizeof(s_search_draft),true,false,UI_INPUT_ENGLISH);}
 simulator_replay(&ctx,gesture_event,on_key,events);
 // 宿主每帧是独立进程；退出前保存内存中的当前位置，模拟真机离页的保存边界。
 // Each host frame is a separate process; persist its current position to model the hardware leave-page save boundary.
 if(s_text&&(s_view==READING||s_view==TOC))save_progress();
 render(&ctx,fb);
 if(ctx.request_app)simulator_route=app_index_of(ctx.request_app);
 simulator_toc_jump=s_toc_jump_open;simulator_book_view=s_view;simulator_reader_panel=s_reader_panel;simulator_reader_fullscreen=s_reader_fullscreen;simulator_reader_page=s_view==READING?s_page:ctx.leaf;simulator_reader_px=s_px;
 size_t pages=s_view==READING?book_layout_page_count():(size_t)leaves();
 /* 退出进程时由 OS 回收；保留路径供 render 输出，不模拟实际关书。
  * OS reclaims this one-frame process; preserve source path for renderer metadata. */
 return pages?pages:1;
}

bool simulator_markdown_committed(void){return s_md_committed;}
unsigned simulator_markdown_size(unsigned index){return index<4?(s_reader_slider>=5?s_md_draft[index]:s_md_sizes[index]):0;}
