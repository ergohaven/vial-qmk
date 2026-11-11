#!/usr/bin/env python3
"""Production player font expressions + marquee, real pinned LVGL/fonts (no hardware)."""
import argparse, re, subprocess
from pathlib import Path
from macropad_native_support import validate_lvgl
ROOT=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('output',type=Path);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
lvgl=ROOT/'lib/lvgl';validate_lvgl(lvgl)
s=(ROOT/'keyboards/ergohaven/sm62/screen_player.c').read_text()
def function(name):
 m=re.search(r'^static [^\n;]+\b'+name+r'\([^;\n]*\)\s*\{',s,re.M);assert m,name
 e=s.index('{',m.start())+1;depth=1
 while depth:depth+=(s[e]=='{')-(s[e]=='}');e+=1
 return s[m.start():e]+'\n'
fonts=[]
for label in ['label_title','label_artist']:
 line=next(x for x in s.splitlines() if label+' = make_label(' in x)
 fonts.append(line.split(', ')[5])
pre='''#include <lvgl.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "player_fonts.h"
LV_FONT_DECLARE(sm62_font_zen_regular_24);
LV_FONT_DECLARE(sm62_font_zen_bold_24);
static lv_obj_t *label_title;
#define TRACK_LABEL_LEFT_PAD 10
#define TRACK_TEXT_AVAILABLE 185
'''.replace('#include "player_fonts.h"', '#include "player_fonts.h"' if (ROOT/'keyboards/ergohaven/sm62/player_fonts.h').exists() else '')
pre+='static const lv_font_t *title_font(void){return '+fonts[0]+';}\nstatic const lv_font_t *artist_font(void){return '+fonts[1]+';}\n'
pre+=function('track_scroll_x')+function('set_track_text')
test=r'''
static lv_color_t frame[240*80];
static void flush(lv_disp_drv_t *d,const lv_area_t *a,lv_color_t *p){for(int y=a->y1;y<=a->y2;y++)for(int x=a->x1;x<=a->x2;x++)frame[y*240+x]=*p++;lv_disp_flush_ready(d);}
static unsigned hash(void){unsigned h=2166136261u;for(size_t i=0;i<sizeof(frame);i++)h=(h^((unsigned char*)frame)[i])*16777619u;return h;}
static void render(void){lv_obj_invalidate(lv_scr_act());lv_refr_now(NULL);}
int main(int argc,char **argv){
 lv_init();static lv_disp_draw_buf_t buf;static lv_color_t pixels[240*20];lv_disp_draw_buf_init(&buf,pixels,0,240*20);
 static lv_disp_drv_t drv;lv_disp_drv_init(&drv);drv.hor_res=240;drv.ver_res=80;drv.draw_buf=&buf;drv.flush_cb=flush;lv_disp_drv_register(&drv);
 lv_obj_set_style_pad_all(lv_scr_act(),0,0);lv_obj_set_style_bg_color(lv_scr_act(),lv_color_black(),0);
 lv_obj_t *viewport=lv_obj_create(lv_scr_act());lv_obj_remove_style_all(viewport);lv_obj_set_size(viewport,195,32);lv_obj_set_pos(viewport,27,0);
 label_title=lv_label_create(viewport);lv_obj_set_style_text_font(label_title,title_font(),0);lv_obj_set_style_text_color(label_title,lv_color_white(),0);lv_obj_set_height(label_title,32);lv_label_set_long_mode(label_title,LV_LABEL_LONG_CLIP);
 const lv_font_t *old[]={&sm62_font_zen_regular_24,&sm62_font_zen_bold_24};const lv_font_t *newf[]={title_font(),artist_font()};
 unsigned checks=0;
 for(int f=0;f<2;f++){
  assert(old[f]->line_height==newf[f]->line_height&&old[f]->base_line==newf[f]->base_line);
  for(unsigned c=32;c<127;c++)for(unsigned n=32;n<127;n++){
   lv_font_glyph_dsc_t x={0},y={0};assert(lv_font_get_glyph_dsc(old[f],&x,c,n));assert(lv_font_get_glyph_dsc(newf[f],&y,c,n));
   assert(x.adv_w==y.adv_w&&x.box_w==y.box_w&&x.box_h==y.box_h&&x.ofs_x==y.ofs_x&&x.ofs_y==y.ofs_y);
   assert(lv_font_get_glyph_bitmap(old[f],c)==lv_font_get_glyph_bitmap(newf[f],c));checks++;
  }
 }
 const char *latin="Latin AV To WAVE 123 abc xyz -- long scrolling original text";
 set_track_text(latin);
 for(int t=0;t<6;t++){
  lv_tick_inc(500);lv_timer_handler();render();unsigned original=hash();
  lv_obj_set_style_text_font(label_title,old[0],0);render();assert(hash()==original);
  lv_obj_set_style_text_font(label_title,newf[0],0);checks++;
 }
 if(argc>1&&strcmp(argv[1],"--latin-only")==0){printf("PASS Latin %u checks\n",checks);return 0;}
 const unsigned fixture[]={0xac00,0xd7a3,0xd55c,0x1100,0x3131,0x3042,0x3093,0x30a2,0x30f3,0x65e5,0x672c,0x97f3,0x4e2d,0x56fd,0x570b,0x9fef,0x3400,0x4db5,0x3002,0xff01};
 for(int f=0;f<2;f++)for(unsigned i=0;i<sizeof(fixture)/sizeof(*fixture);i++){
  lv_font_glyph_dsc_t d={0};bool ok=lv_font_get_glyph_dsc(newf[f],&d,fixture[i],0);
  if(!ok||d.is_placeholder||!d.box_w||!d.box_h){fprintf(stderr,"MISSING CJK U+%04X field=%d\n",fixture[i],f);return 1;}
  assert(lv_font_get_glyph_bitmap(d.resolved_font,fixture[i]));if(d.ofs_y+d.box_h>newf[f]->line_height-newf[f]->base_line){fprintf(stderr,"BOUNDS U+%04X field=%d h=%d y=%d resolved=%s\n",fixture[i],f,d.box_h,d.ofs_y,d.resolved_font==old[f]?"original":"fallback");return 1;}checks++;
 }
 const unsigned ranges[][2]={{0x1100,0x11ff},{0x3000,0x303f},{0x3041,0x3096},{0x3099,0x30ff},{0x3131,0x318e},{0x3400,0x4db5},{0x4e00,0x9fef},{0xac00,0xd7a3},{0xff01,0xffbe},{0xffc2,0xffc7},{0xffca,0xffcf},{0xffd2,0xffd7},{0xffda,0xffdc},{0xffe0,0xffe6},{0xffe8,0xffee}};
 unsigned covered=0,clipped=0;
 for(int f=0;f<2;f++)for(unsigned r=0;r<sizeof(ranges)/sizeof(*ranges);r++)for(unsigned cp=ranges[r][0];cp<=ranges[r][1];cp++){
  lv_font_glyph_dsc_t d={0};assert(lv_font_get_glyph_dsc(newf[f],&d,cp,0)&&!d.is_placeholder);
  if(d.box_w&&d.box_h)assert(lv_font_get_glyph_bitmap(d.resolved_font,cp));
  int top=newf[f]->line_height-newf[f]->base_line-d.box_h-d.ofs_y;
  if(top<0||top+d.box_h>32)clipped++;
  covered++;
 }
 assert(covered==79116);printf("CJK coverage: %u field/codepoint lookups, %u outside 32px (explicit field bounds reported)\n",covered,clipped);
 const unsigned unsupported[]={0x9ff0,0x9fff,0x4db6,0x4dbf,0x20000,0x1f600};
 for(unsigned i=0;i<sizeof(unsupported)/sizeof(*unsupported);i++){lv_font_glyph_dsc_t d={0};assert(!lv_font_get_glyph_dsc(newf[0],&d,unsupported[i],0)||d.is_placeholder);}
 set_track_text("Latin 한국어 日本語 カタカナ ひらがな 简体中文 繁體中文 -- long title");lv_obj_update_layout(label_title);
 assert(lv_obj_get_width(label_title)>185);unsigned hashes[4];int xs[4];
 for(int t=0;t<4;t++){lv_tick_inc(1000);lv_timer_handler();render();hashes[t]=hash();xs[t]=lv_obj_get_x(label_title);}
 assert(xs[0]>xs[1]&&xs[1]>xs[2]&&xs[2]>xs[3]);assert(hashes[0]!=hashes[1]&&hashes[1]!=hashes[2]);
 FILE *out=fopen("frame.ppm","wb");fprintf(out,"P6\n240 80\n255\n");for(unsigned i=0;i<240*80;i++){lv_color32_t c; c.full=lv_color_to32(frame[i]);fputc(c.ch.red,out);fputc(c.ch.green,out);fputc(c.ch.blue,out);}fclose(out);
 printf("PASS %u Latin metrics/bitmap + CJK glyph checks; mixed marquee x=%d,%d,%d,%d; frame hashes=%08x,%08x,%08x,%08x\n",checks,xs[0],xs[1],xs[2],xs[3],hashes[0],hashes[1],hashes[2],hashes[3]);
 set_track_text("Short");lv_obj_update_layout(label_title);render();unsigned short_hash=hash();
 lv_mem_monitor_t before,after;lv_mem_monitor(&before);
 for(unsigned i=0;i<1000;i++){
  set_track_text(i%2?"한국어 日本語 简体中文 繁體中文 -- long title with changing 漢字":"カタカナ 한글 long changing Latin 123 -- full title");
  lv_tick_inc(200);lv_timer_handler();render();
  set_track_text("");render();set_track_text("Short");lv_obj_update_layout(label_title);render();
  assert(lv_obj_get_x(label_title)==TRACK_LABEL_LEFT_PAD);assert(hash()==short_hash);assert(lv_mem_test()==LV_RES_OK);
 }
 lv_mem_monitor(&after);assert(before.free_size==after.free_size);printf("PASS 1000 long/change/clear/short cycles; exact short pixels restored; LVGL free bytes %u -> %u\n",(unsigned)before.free_size,(unsigned)after.free_size);
}
'''
src=a.output/'player.c';src.write_text(pre+test)
fontdir=ROOT/'keyboards/ergohaven/macropad'
extra=list((ROOT/'keyboards/ergohaven/sm62').glob('player_fonts.c'))+list((ROOT/'keyboards/ergohaven/sm62').glob('sm62_font_cjk_24*.c'))
cmd=['gcc','-std=gnu11','-O1','-g','-Wall','-Wextra','-Werror','-fsanitize=undefined','-fno-sanitize-recover=all','-DLV_CONF_SKIP','-DLV_USE_FONT_COMPRESSED=1','-DLV_USE_ASSERT_MALLOC=1','-DLV_USE_LOG=1','-I'+str(lvgl),'-I'+str(ROOT/'keyboards/ergohaven/sm62'),str(src),str(fontdir/'sm62_font_zen_regular_24.c'),str(fontdir/'sm62_font_zen_bold_24.c'),*map(str,extra),*sorted(str(f) for f in (lvgl/'src').rglob('*.c') if f.name!='lv_font_fmt_txt.c'),str(a.output/'lv_font_fmt_txt.o'),'-lm','-o',str(a.output/'player')]
# Pinned LVGL's RLE decoder shifts a negative integer at line 506. Keep
# all other UBSan checks, disabling shift only for that unchanged dependency.
subprocess.run(['gcc','-std=gnu11','-O1','-g','-fsanitize=undefined','-fno-sanitize=shift','-fno-sanitize-recover=all','-DLV_CONF_SKIP','-DLV_USE_FONT_COMPRESSED=1','-I'+str(lvgl),'-c',str(lvgl/'src/font/lv_font_fmt_txt.c'),'-o',str(a.output/'lv_font_fmt_txt.o')],check=True)
(a.output/'command.txt').write_text(' '.join(cmd));subprocess.run(cmd,check=True)
r=subprocess.run([str(a.output.resolve()/'player')],cwd=a.output,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT);(a.output/'results.log').write_text(r.stdout);print(r.stdout,end='');raise SystemExit(r.returncode)
