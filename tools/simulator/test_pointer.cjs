// SPDX-License-Identifier: Apache-2.0
// Exercise the shipped pointer handlers with deterministic browser events/timers.
// 用确定性事件和时钟验证实际页面的鼠标处理器，不依赖真实TF卡或等待。
const {readFileSync}=require('node:fs');
const {join}=require('node:path');
const vm=require('node:vm');
const test=require('node:test');
const assert=require('node:assert/strict');
const html=readFileSync(join(__dirname,'index.html'),'utf8');
const start=html.indexOf('    // Match native ui_kit.h:');
const end=html.indexOf("    document.getElementById('file').onchange",start);
assert.ok(start>=0&&end>start);
const handlers=html.slice(start,end);

function preview(zoom=false){
  let now=0,nextId=0;
  const timers=new Map(),captures=new Set(),sent=[],listeners={};
  const canvas={getBoundingClientRect:()=>({left:0,top:0,width:684,height:1216}),
    setPointerCapture:id=>captures.add(id),hasPointerCapture:id=>captures.has(id),
    releasePointerCapture:id=>{captures.delete(id);canvas.onlostpointercapture?.({pointerId:id})}};
  vm.runInNewContext(handlers,{canvas,state:{document_zoom:zoom},busy:false,polling:false,
    Date:{now:()=>now},Math,send:data=>{sent.push({...data});return Promise.resolve()},
    setTimeout:(fn,ms)=>{const id=++nextId;timers.set(id,{fn,at:now+ms});return id},
    clearTimeout:id=>timers.delete(id),window:{addEventListener:(kind,fn)=>listeners[kind]=fn}});
  function event(x=150,y=260,button=0,id=1){return {clientX:x,clientY:y,button,pointerId:id,isPrimary:true,preventDefault(){this.prevented=true}}}
  return {sent,canvas,event,listeners,
    advance(ms){now+=ms;for(const [id,timer]of timers)if(timer.at<=now){timers.delete(id);timer.fn()}},
    down:(...args)=>canvas.onpointerdown(event(...args)),
    move:(...args)=>canvas.onpointermove(event(...args)),
    up:(...args)=>canvas.onpointerup(event(...args)),
    kinds:()=>sent.map(e=>e.type)};
}

test('short left click emits one tap',async()=>{
  const p=preview();p.down();p.advance(499);assert.deepEqual(p.kinds(),[]);
  await p.up();p.advance(1000);assert.deepEqual(p.kinds(),['t']);
});
test('holding fires at 500ms, before release, without a second click',async()=>{
  const p=preview();p.down();p.advance(500);assert.deepEqual(p.kinds(),['l']);
  p.advance(2000);await p.up();assert.deepEqual(p.kinds(),['l']);
});
test('movement cancels hold even after returning to the starting point',async()=>{
  const p=preview();p.down();p.move(200,260);p.move(150,260);p.advance(600);
  assert.deepEqual(p.kinds(),[]);await p.up();assert.deepEqual(p.kinds(),['c']);
});
test('vertical drag emits swipe instead of long press',async()=>{
  const p=preview();p.down(342,1000);p.move(342,400);p.advance(600);
  await p.up(342,400);assert.deepEqual(p.kinds(),['u']);
});
test('right click suppresses browser menu and emits only one long press',async()=>{
  const p=preview();p.down(150,260,2);const e=p.event(150,260,2);
  p.canvas.oncontextmenu(e);assert.equal(e.prevented,true);await p.up(150,260,2);
  p.advance(1000);assert.deepEqual(p.kinds(),['l']);
});
test('context menu after a touch hold does not fire twice',async()=>{
  const p=preview();p.down();p.advance(500);p.canvas.oncontextmenu(p.event());
  await p.up();assert.deepEqual(p.kinds(),['l']);
});
test('cancel, loss of capture and window blur stop a pending long press',()=>{
  for(const cancel of [p=>p.canvas.onpointercancel(p.event()),
    p=>p.canvas.onlostpointercapture(p.event()),p=>p.listeners.blur()]){
    const p=preview();p.down();cancel(p);p.advance(600);assert.deepEqual(p.kinds(),[]);
  }
});
test('other mouse buttons and unrelated pointers do not release a held contact',async()=>{
  const p=preview();p.down(150,260,1);p.advance(600);assert.deepEqual(p.kinds(),[]);
  p.down();await p.up(150,260,0,2);p.advance(500);assert.deepEqual(p.kinds(),['l']);
  await p.up();assert.deepEqual(p.kinds(),['l']);
});
test('zoom drag preserves final displacement without long press or exit tap',async()=>{
  const p=preview(true);p.down(342,1000);p.advance(600);p.move(342,400);
  await p.up(342,300);assert.deepEqual(p.kinds(),['p','m','m','c']);
  assert.equal(p.sent[2].yy,300);
  const click=preview(true);click.down();await click.up();assert.deepEqual(click.kinds(),['p','t']);
});
