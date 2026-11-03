#pragma once
/*
 * dash_page.h - laptop dashboard served by the ESP32 at http://192.168.4.1/
 * Fully self-contained (no CDN / web fonts: the laptop has no internet on the dash's Wi-Fi).
 *
 * Keep SIGS (frame decoding) in sync with SIGS[] in can_functions.cpp, and DEFS
 * (ranges / good zones) in sync with DEFS[] in sensor_functions.cpp.
 */
#include <Arduino.h>

const char DASH_PAGE[] PROGMEM = R"KD(<!DOCTYPE html><html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><title>SDSMT Formula SAE</title>
<style>
:root{--bg:#06080c;--panel:#0d1219;--line:#1c2634;--txt:#eaf0f7;--dim:#7d8ca0;--ok:#2fd27a;--warn:#ff9f1a;--crit:#ff3b3b;--acc:#4cc9f0}
*{box-sizing:border-box}
html,body{margin:0;min-height:100%}
body{background:radial-gradient(1300px 800px at 50% -15%,#121c2b,var(--bg));color:var(--txt);
  font-family:"Segoe UI",system-ui,-apple-system,Roboto,sans-serif;display:flex;flex-direction:column;min-height:100vh}
header{display:flex;gap:10px;align-items:center;padding:10px 16px;border-bottom:1px solid var(--line);flex-wrap:wrap;background:rgba(8,11,16,.7)}
h1{font-size:15px;letter-spacing:.25em;margin:0 14px 0 0;color:var(--dim);font-weight:600}
.pill{padding:3px 12px;border-radius:99px;font-size:12px;font-weight:700;letter-spacing:.08em;background:#3a1212;color:#ff8a8a;border:1px solid #5c1c1c}
.pill.ok{background:#0e2e1f;color:#6df0a8;border-color:#1a5236}
.pill.mid{background:#33260a;color:#ffc25a;border-color:#5c4210}
.sp{flex:1}
button,label.chk{font:inherit;font-size:13px;color:var(--txt);background:#141c27;border:1px solid var(--line);border-radius:8px;padding:6px 12px;cursor:pointer}
button:hover{border-color:#34465c}
button.on{background:#7a1414;border-color:#c33}
label.chk{display:flex;gap:6px;align-items:center}
#info{font-size:12px;color:var(--dim);min-width:80px;text-align:right}
#alerts{display:flex;gap:8px;justify-content:center;flex-wrap:wrap;min-height:46px;padding:8px 16px;align-items:center}
.clear{color:#3d4b5e;font-size:13px;letter-spacing:.3em}
.chip{padding:6px 14px;border-radius:8px;font-weight:700;font-size:14px;border:2px solid;letter-spacing:.04em}
.chip span{font-weight:500;opacity:.9;margin-left:6px}
.chip.warn{color:var(--warn);border-color:var(--warn);animation:fl 1s steps(1) infinite}
.chip.crit{color:var(--crit);border-color:var(--crit);animation:fl .36s steps(1) infinite}
.chip.go{color:var(--ok);border-color:var(--ok);animation:fl .36s steps(1) infinite}
.chip.arm{color:#fff;border-color:#fff}
@keyframes fl{50%{opacity:.2}}
main{flex:1;display:grid;grid-template-columns:minmax(0,1fr) minmax(360px,540px) minmax(0,1fr);gap:14px;padding:0 16px 10px}
.col{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));grid-auto-rows:1fr;gap:12px;align-content:start}
.card{background:linear-gradient(180deg,#111822,var(--panel));border:1px solid var(--line);border-radius:12px;padding:12px 14px 14px;position:relative;transition:border-color .2s}
.card .lab{font-size:12px;letter-spacing:.14em;color:var(--dim);font-weight:600}
.card .val{display:flex;align-items:baseline;gap:6px;margin:6px 0 12px;white-space:nowrap}
.card b{font-size:clamp(26px,3.1vw,44px);font-weight:700;font-variant-numeric:tabular-nums;line-height:1}
.card small{color:var(--dim);font-size:13px}
.zbar{height:10px;border-radius:5px;position:relative}
.zbar i{position:absolute;top:-4px;width:4px;height:18px;border-radius:2px;background:#fff;transform:translateX(-50%);box-shadow:0 0 6px rgba(255,255,255,.5)}
.card.stale{opacity:.4}
.card.warn{border-color:var(--warn);box-shadow:0 0 18px rgba(255,159,26,.18)}
.card.crit{border-color:var(--crit);box-shadow:0 0 22px rgba(255,59,59,.3);animation:cf .5s steps(1) infinite}
.card.warn b{color:var(--warn)}.card.crit b{color:var(--crit)}
.card.go b{color:var(--ok)}.card.arm b{color:#fff}
@keyframes cf{50%{border-color:#5c1c1c}}
.center{background:linear-gradient(180deg,#111822,var(--panel));border:1px solid var(--line);border-radius:16px;padding:12px 12px 14px;display:flex;flex-direction:column;align-items:center}
#shift{display:flex;gap:6px;margin:2px 0 4px;width:78%}
#shift i{flex:1;height:10px;border-radius:5px;background:#1a2230}
#tach{width:100%;height:auto}
#spdbar{display:flex;gap:3px;width:86%;margin-top:6px}
#spdbar i{flex:1;height:12px;border-radius:3px;background:#1a2230}
.panel{margin:0 16px 16px;background:linear-gradient(180deg,#111822,var(--panel));border:1px solid var(--line);border-radius:12px;padding:10px 14px 12px}
.gh{display:flex;gap:6px;flex-wrap:wrap;align-items:center;margin-bottom:8px}
.gh h2{font-size:12px;letter-spacing:.14em;color:var(--dim);margin:0 10px 0 0;font-weight:600}
.tg{font-size:11px;padding:3px 9px;border-radius:99px;background:transparent;border:1px solid var(--line);color:var(--dim)}
.tg.on{color:#06080c;font-weight:700}
#gr{width:100%;height:220px;display:block}
.ax{display:flex;justify-content:space-between;font-size:11px;color:#51607a;margin-top:2px}
.lk{display:grid;grid-template-columns:repeat(auto-fit,minmax(210px,1fr));gap:16px;align-items:start}
.lk .lab{font-size:12px;letter-spacing:.14em;color:var(--dim);font-weight:600}
.lk .big{display:flex;align-items:baseline;gap:6px;margin:8px 0 4px;white-space:nowrap}
.lk .big b{font-size:clamp(26px,3vw,40px);font-weight:700;font-variant-numeric:tabular-nums;line-height:1}
.lk small{color:var(--dim);font-size:13px}
#bars{display:inline-flex;gap:3px;align-items:flex-end;height:30px;margin-right:6px}
#bars i{width:7px;border-radius:2px;background:#1a2230}
.cal{font-size:12px;color:var(--dim);display:grid;gap:6px}
.cal div{display:flex;gap:6px;align-items:center;flex-wrap:wrap}
.cal input,.cal select{font:inherit;font-size:13px;background:#0a0f15;color:var(--txt);border:1px solid var(--line);border-radius:6px;padding:4px 6px;width:72px}
.cal select{width:58px}
@media(max-width:1100px){main{grid-template-columns:1fr}.center{order:-1}.col{grid-template-columns:repeat(3,minmax(0,1fr))}}
@media(max-width:600px){.col{grid-template-columns:repeat(2,minmax(0,1fr))}}
</style></head><body>
<header><h1>KART DASH</h1><span id="st" class="pill">OFFLINE</span><span id="info"></span><span class="sp"></span>
<label class="chk"><input type="checkbox" id="rawon"> raw frames</label>
<button id="rec">&#9679; Record</button><button id="save">Save CSV</button><button id="demo">Demo</button><button id="fs">Fullscreen</button></header>
<div id="alerts"></div>
<main>
<div class="col" id="colL"></div>
<div class="center">
  <div id="shift"></div>
  <svg id="tach" viewBox="0 0 420 380"></svg>
  <div id="spdbar"></div>
</div>
<div class="col" id="colR"></div>
</main>
<div class="panel"><div class="gh"><h2>WIRELESS LINK</h2><span id="lq" class="pill">NO LINK DATA</span></div>
<div class="lk">
 <div><div class="lab">SIGNAL (AS SEEN BY THE KART)</div><div class="big"><span id="bars"></span><b id="dbm">--</b><small>dBm</small></div><div class="lab" id="wmin"></div></div>
 <div><div class="lab">ESTIMATED DISTANCE</div><div class="big"><b id="dist">--</b><small class="du">ft</small></div><div class="lab" id="drng"></div></div>
 <div><div class="lab">FARTHEST THIS SESSION</div><div class="big"><b id="dmax">--</b><small class="du">ft</small></div><div class="lab" id="drop"></div></div>
 <div class="cal"><div class="lab">CALIBRATE (stand still for 5 s)</div>
  <div>I am <input id="cd" type="number" value="100" min="1"> <select id="cu"><option>ft</option><option>m</option></select> from the kart</div>
  <div><button id="calb">Set reference</button><button id="clr">Clear</button><button id="rst">Reset max</button></div>
  <div>Path loss n <input id="cn" type="number" step="0.1" min="1.5" max="4" value="2.5"></div>
  <div id="cs"></div></div>
</div></div>
<div class="panel"><div class="gh"><h2>LIVE GRAPH (last 60 s, each line scaled to its own range)</h2><span id="tgs" class="gh" style="margin:0"></span></div>
<canvas id="gr"></canvas><div class="ax"><span>-60 s</span><span>-30 s</span><span>now</span></div></div>
<script>
const MAXRPM=14000,SHIFT=11000,REDL=12500,SPDMAX=100,HN=1200,TACH_N=40;
// [id, startBit, bits, signed, scale, offset, key]   (mirror of SIGS[] in can_functions.cpp)
const SIGS=[
[0x640,0,16,0,1,0,'rpm'],[0x640,32,16,1,0.18,32,'iat'],[0x641,16,16,0,0.147,0,'afr'],
[0x641,32,16,0,0.0145038,0,'fuelp'],[0x642,32,16,1,-0.1,0,'adv'],[0x644,48,16,0,0.0145038,0,'oilp'],
[0x645,0,16,1,0.0145038,0,'boost'],[0x648,0,16,0,0.0621371,0,'mph'],[0x649,0,8,0,1.8,-40,'clt'],
[0x649,8,8,0,1.8,-40,'oilt'],[0x649,40,8,0,0.1,0,'batt'],[0x64A,0,16,0,0.18,32,'egt'],
[0x64B,0,8,0,0.1,0,'knock'],[0x64D,52,4,1,1,0,'gear'],[0x64E,26,1,0,1,0,'launch']];
// [key, name, unit, min, max, goodLo, goodHi, decimals, isState]   (mirror of DEFS[] in sensor_functions.cpp)
const DEFS=[
['oilt','OIL TEMP','\u00B0F',100,300,160,260,0,0],['clt','COOLANT','\u00B0F',100,260,150,220,0,0],
['adv','ADVANCE','deg',-5,45,5,38,1,0],['knock','KNOCK','deg RET',0,10,0,1.5,1,0],
['launch','LAUNCH','',0,2,0,2,0,1],['oilp','OIL PRESS','psi',0,100,25,80,0,0],
['batt','BATTERY','volts',8,16,12,14.8,1,0],['afr','AFR','a/f',10,18,11.5,15.2,1,0],
['boost','BOOST','psi',-10,20,-10,15,1,0],['egt','EGT','\u00B0F',800,1800,900,1500,0,0],
['iat','INTAKE T','\u00B0F',30,200,30,140,0,0],['fuelp','FUEL PRESS','psi',0,80,35,60,0,0]];

const $=id=>document.getElementById(id);
const ALLKEYS=['rpm','mph','gear',...DEFS.map(d=>d[0]),'rssi','dist'];
const NAMES={rpm:'RPM',mph:'SPEED',gear:'GEAR',rssi:'WIFI dBm',dist:'DIST (m)'};DEFS.forEach(d=>NAMES[d[0]]=d[1]);
const RNG={rpm:[0,MAXRPM],mph:[0,SPDMAX],gear:[0,6],rssi:[-95,-30],dist:[0,250]};DEFS.forEach(d=>RNG[d[0]]=[d[3],d[4]]);
const PAL=['#4cc9f0','#ffb703','#80ed99','#f72585','#b5179e','#fb8500','#90e0ef','#caffbf','#ff6b6b','#a0c4ff','#ffd6a5','#bdb2ff','#9bf6ff','#fdffb6','#ffadad'];
const COL={};ALLKEYS.forEach((k,i)=>COL[k]=PAL[i%PAL.length]);

const val={},seen={};
let nfr=0,lastN=0,fps=0,lastEsp=0,connected=false,rec=false,rawOn=false,rows=[],raw=[],demo=false;

// ---------- frame decoding ----------
function dec(s,dv,o,dlc){
  const [id,bit,len,sg,sc,off]=s;
  if(Math.ceil((bit+len)/8)>dlc)return null;
  let r;
  if(len<8){r=(dv.getUint8(o+(bit>>3))>>(bit&7))&((1<<len)-1);}
  else{r=0;for(let i=0;i<len/8;i++)r=r*256+dv.getUint8(o+(bit>>3)+i);
    const all=Math.pow(2,len)-1,smax=Math.floor(all/2);
    if(sg?r===smax:r===all)return null;}              // "channel not configured"
  let v=r;if(sg&&r>=Math.pow(2,len-1))v=r-Math.pow(2,len);
  return v*sc+off;
}
function onFrames(buf){
  const dv=new DataView(buf),now=performance.now();
  for(let o=0;o+17<=buf.byteLength;o+=17){
    const t=dv.getUint32(o,true),id=dv.getUint32(o+4,true),dlc=Math.min(dv.getUint8(o+8),8);
    nfr++;lastEsp=t;
    if(rec&&rawOn){let h='';for(let i=0;i<dlc;i++)h+=dv.getUint8(o+9+i).toString(16).padStart(2,'0');
      raw.push(t+',0x'+id.toString(16)+','+dlc+','+h);}
    for(const s of SIGS)if(s[0]===id){const v=dec(s,dv,o+9,dlc);
      if(v!==null){const k=s[6];val[k]=k==='gear'?Math.min(6,Math.max(0,Math.round(v))):v;seen[k]=now;}}
  }
}

// ---------- tachometer (SVG) ----------
const NS='http://www.w3.org/2000/svg',CX=210,CY=190,RO=172,RI=136,T0=210,SWEEP=240;
const P=(r,a)=>[CX+r*Math.cos(a*Math.PI/180),CY-r*Math.sin(a*Math.PI/180)];
const tach=$('tach'),segs=[];
function mk(tag,attrs,txt){const e=document.createElementNS(NS,tag);for(const k in attrs)e.setAttribute(k,attrs[k]);if(txt!==undefined)e.textContent=txt;tach.appendChild(e);return e;}
for(let i=0;i<TACH_N;i++){
  const a0=T0-i*SWEEP/TACH_N,a1=T0-(i+1)*SWEEP/TACH_N+0.8;
  const p0=P(RO,a0),p1=P(RO,a1),p2=P(RI,a1),p3=P(RI,a0);
  segs.push(mk('path',{d:`M${p0}A${RO} ${RO} 0 0 1 ${p1}L${p2}A${RI} ${RI} 0 0 0 ${p3}Z`,fill:'#1a2230'}));
}
for(let k=0;k<=14;k+=2){const a=T0-k/14*SWEEP,p=P(RO+16,a);
  mk('text',{x:p[0],y:p[1]+4,'text-anchor':'middle','font-size':13,fill:k*1000>=REDL?'#ff5a5a':'#7d8ca0'},k);}
const gearT=mk('text',{x:CX,y:CY+30,'text-anchor':'middle','font-size':128,'font-weight':700,fill:'#fff'},'N');
const rpmT =mk('text',{x:CX,y:CY+72,'text-anchor':'middle','font-size':34,'font-weight':600,fill:'#fff'},'0');
mk('text',{x:CX,y:CY+92,'text-anchor':'middle','font-size':12,fill:'#7d8ca0','letter-spacing':3},'RPM');
const spdT =mk('text',{x:CX,y:CY+168,'text-anchor':'middle','font-size':58,'font-weight':700,fill:'#fff'},'0');
mk('text',{x:CX,y:CY+188,'text-anchor':'middle','font-size':12,fill:'#7d8ca0','letter-spacing':3},'MPH');
const segCol=i=>{const r=(i+1)/TACH_N*MAXRPM;return r>=REDL?'#ff3b3b':r>=SHIFT?'#ffd60a':'#ffffff';};
let prevLit=-1;
function renderTach(){
  const r=val.rpm||0,lit=Math.max(0,Math.min(TACH_N,Math.round(r/MAXRPM*TACH_N)));
  if(lit!==prevLit){segs.forEach((s,i)=>s.setAttribute('fill',i<lit?segCol(i):'#1a2230'));prevLit=lit;}
  rpmT.textContent=Math.round(r);
  const c=r>=REDL?'#ff3b3b':r>=SHIFT?'#ffd60a':'#ffffff';
  rpmT.setAttribute('fill',c);gearT.setAttribute('fill',c);
  const g=val.gear;gearT.textContent=g===undefined?'-':(g===0?'N':g);
  spdT.textContent=Math.round(val.mph||0);
}
// shift lights (8000 rpm -> SHIFT_RPM) and speed bar
const sh=$('shift'),shL=[];for(let i=0;i<12;i++){const e=document.createElement('i');sh.appendChild(e);shL.push(e);}
const sb=$('spdbar'),sbL=[];for(let i=0;i<25;i++){const e=document.createElement('i');sb.appendChild(e);sbL.push(e);}
let prevSh=-2,prevSb=-1;
function renderBars(now){
  const r=val.rpm||0;let n=Math.max(0,Math.min(12,Math.floor((r-8000)/(SHIFT-8000)*12)));
  const flash=r>=SHIFT&&((now/120)&1);
  const key=n+(flash?'f':'');
  if(key!==prevSh){prevSh=key;
    shL.forEach((e,i)=>{const c=i<4?'#2fd27a':i<8?'#ffd60a':'#ff3b3b';e.style.background=(i<n&&!(r>=SHIFT&&flash))?c:'#1a2230';});}
  const m=Math.max(0,Math.min(25,Math.round((val.mph||0)/SPDMAX*25)));
  if(m!==prevSb){prevSb=m;sbL.forEach((e,i)=>e.style.background=i<m?(i>=22?'#ffd60a':'#4cc9f0'):'#1a2230');}
}

// ---------- sensor cards + alerts ----------
const cards={},act={},colL=$('colL'),colR=$('colR');
DEFS.forEach((d,i)=>{
  const [k,n,u,mn,mx,gl,gh,dc,st]=d;
  const a=Math.max(0,(gl-mn)/(mx-mn)*100),b=Math.min(100,(gh-mn)/(mx-mn)*100);
  const el=document.createElement('div');el.className='card';
  el.innerHTML='<div class="lab">'+n+'</div><div class="val"><b>--</b><small>'+u+'</small></div>'+
    (st?'':'<div class="zbar" style="background:linear-gradient(90deg,#4a1c1c 0 '+a+'%,#1f4d3a '+a+'% '+b+'%,#4a1c1c '+b+'% 100%)"><i></i></div>');
  (i<6?colL:colR).appendChild(el);cards[k]=el;
});
const alBox=$('alerts');let alKey='',alEls={};
function renderCards(now){
  const al=[];
  DEFS.forEach(d=>{
    const [k,n,u,mn,mx,gl,gh,dc,st]=d,el=cards[k],v=val[k],has=v!==undefined,stale=has&&now-seen[k]>1000;
    const b=el.querySelector('b'),m=el.querySelector('i');let cls='card'+(stale?' stale':'');
    if(!has){el.className=cls;b.textContent='--';if(m)m.style.display='none';return;}
    if(st){
      const s=Math.min(2,Math.max(0,Math.round(v)));b.textContent=['OFF','ARM','GO!'][s];
      if(s===1){cls+=' arm';al.push([k,n,'ARM','arm',0]);}
      if(s===2){cls+=' go';al.push([k,n,'GO!','go',2]);}
    }else{
      b.textContent=v.toFixed(dc);
      const span=gh-gl,h=span*0.03,out=v<gl||v>gh,back=v>=gl+h&&v<=gh-h;
      if(!act[k]){if(out)act[k]=true;}else if(back)act[k]=false;
      if(act[k]){const crit=v<gl-span*0.25||v>gh+span*0.25;
        cls+=crit?' crit':' warn';al.push([k,n,v.toFixed(dc)+' '+u,crit?'crit':'warn',crit?3:1]);}
      m.style.display='';m.style.left=(Math.max(0,Math.min(1,(v-mn)/(mx-mn)))*100)+'%';
    }
    el.className=cls;
  });
  al.sort((a,b)=>b[4]-a[4]);const top=al.slice(0,6);
  const key=top.map(a=>a[0]+a[3]).join('|');
  if(key!==alKey){alKey=key;alEls={};alBox.innerHTML='';
    if(!top.length)alBox.innerHTML='<div class="clear">ALL CLEAR</div>';
    top.forEach(a=>{const c=document.createElement('div');c.className='chip '+a[3];
      c.innerHTML=a[1]+'<span></span>';alBox.appendChild(c);alEls[a[0]]=c.querySelector('span');});}
  top.forEach(a=>{if(alEls[a[0]])alEls[a[0]].textContent=a[2];});
}

// ---------- graph ----------
const hist={};ALLKEYS.forEach(k=>hist[k]=new Float32Array(HN).fill(NaN));let hp=0;
const sel=new Set(['rpm','mph','clt','oilp']);
const cv=$('gr'),c2=cv.getContext('2d'),tgs=$('tgs');
ALLKEYS.forEach(k=>{const b=document.createElement('button');b.className='tg';b.textContent=NAMES[k];
  const paint=()=>{const on=sel.has(k);b.classList.toggle('on',on);b.style.background=on?COL[k]:'transparent';b.style.borderColor=on?COL[k]:'';};
  b.onclick=()=>{sel.has(k)?sel.delete(k):sel.add(k);paint();};paint();tgs.appendChild(b);});
function drawGraph(){
  const dpr=window.devicePixelRatio||1,W=cv.clientWidth,H=cv.clientHeight;
  if(!W||!H)return;
  if(cv.width!==Math.round(W*dpr)||cv.height!==Math.round(H*dpr)){cv.width=Math.round(W*dpr);cv.height=Math.round(H*dpr);}
  c2.setTransform(dpr,0,0,dpr,0,0);c2.clearRect(0,0,W,H);
  c2.strokeStyle='#1c2634';c2.lineWidth=1;c2.beginPath();
  for(let i=1;i<4;i++){const y=Math.round(H*i/4)+.5;c2.moveTo(0,y);c2.lineTo(W,y);}
  for(let i=1;i<6;i++){const x=Math.round(W*i/6)+.5;c2.moveTo(x,0);c2.lineTo(x,H);}
  c2.stroke();
  sel.forEach(k=>{
    const [lo,hi]=RNG[k],a=hist[k];c2.strokeStyle=COL[k];c2.lineWidth=2;c2.beginPath();let pen=false;
    for(let i=0;i<HN;i++){const v=a[(hp+i)%HN];if(v!==v){pen=false;continue;}
      const x=i/(HN-1)*W,y=H-4-(Math.min(hi,Math.max(lo,v))-lo)/(hi-lo)*(H-8);
      if(pen)c2.lineTo(x,y);else{c2.moveTo(x,y);pen=true;}}
    c2.stroke();});
}

// ---------- demo data (same idea as SIMULATE on the dash) ----------
function demoStep(){
  const t=performance.now()/1000,now=performance.now(),STAGE=3.5,stage=Math.floor(t/STAGE)%7,cyc=(t%STAGE)/STAGE;
  const GEARS=[1,0,2,3,4,5,6],RATIO=[0,3.5,5,6.5,8,9.5,11];
  const g=GEARS[stage];val.gear=g;val.rpm=2500+cyc*cyc*9000;
  val.mph=g>0?Math.min(SPDMAX,val.rpm/1000*RATIO[g]):(val.mph||0)*0.99;
  DEFS.forEach((d,i)=>{const [k,n,u,mn,mx,gl,gh,dc,st]=d;
    val[k]=st?Math.floor(t/3)%3:Math.max(mn,Math.min(mx,(gl+gh)/2+(gh-gl)/2*1.3*Math.sin(t*(0.15+0.05*i)+i)));});
  for(const k in val)seen[k]=now;nfr+=30;
  if(++demoN%10===1)onStatus({t:'s',rssi:[Math.round(-48-28*(0.5+0.5*Math.sin(t/9))+Math.random()*4-2)],drop:0});
}

// ---------- logging ----------
function logRow(){rows.push([new Date().toISOString(),lastEsp,...ALLKEYS.map(k=>val[k]===undefined?'':val[k].toFixed(2))].join(','));}
function dl(name,text){const a=document.createElement('a');a.href=URL.createObjectURL(new Blob([text],{type:'text/csv'}));a.download=name;a.click();}
$('rec').onclick=function(){rec=!rec;rawOn=$('rawon').checked;this.classList.toggle('on',rec);
  this.innerHTML=rec?'&#9632; Stop':'&#9679; Record';if(rec){rows=[];raw=[];}};
$('save').onclick=()=>{const ts=new Date().toISOString().replace(/[:.]/g,'-');
  dl('kart_'+ts+'.csv','time,esp_ms,'+ALLKEYS.join(',')+'\n'+rows.join('\n'));
  if(raw.length)dl('kart_raw_'+ts+'.csv','esp_ms,id,len,data\n'+raw.join('\n'));};
$('demo').onclick=function(){demo=!demo;this.classList.toggle('on',demo);};
$('fs').onclick=()=>{document.fullscreenElement?document.exitFullscreen():document.documentElement.requestFullscreen();};

// ---------- wireless link: signal strength + rough distance ----------
// The kart sends {"t":"s","rssi":[dBm,...],"drop":n} twice a second. Distance uses a log-distance
// path-loss model: d = d0 * 10^((rssi0 - rssi) / (10 n)), where rssi0 is measured at a known distance d0.
const M_PER_FT=0.3048,DEF_REF={rssi:-45,d:1};
let ref=null,pln=2.5,useFt=true,maxD=0,minR=0,espDrop=0,linkSeen=0,demoN=0;const rs=[];
try{ref=JSON.parse(localStorage.getItem('kdref')||'null');pln=parseFloat(localStorage.getItem('kdn'))||2.5;useFt=localStorage.getItem('kdu')!=='m';}catch(e){}
if(ref&&typeof ref.rssi!=='number')ref=null;
const barEls=[];[8,13,18,24,30].forEach(h=>{const e=document.createElement('i');e.style.height=h+'px';$('bars').appendChild(e);barEls.push(e);});
const setBars=(n,c)=>barEls.forEach((e,i)=>e.style.background=i<n?c:'#1a2230');
const med=a=>{const s=a.slice().sort((x,y)=>x-y);return s[s.length>>1];};
const dAt=(r,n,rf)=>rf.d*Math.pow(10,(rf.rssi-r)/(10*n));
const fmtD=m=>{const v=useFt?m/M_PER_FT:m;return v<10?v.toFixed(1):String(Math.round(v));};
const save=(k,v)=>{try{localStorage.setItem(k,v);}catch(e){}};
function onStatus(o){
  if(!o||o.t!=='s')return;
  espDrop=o.drop||0;
  if(!o.rssi||!o.rssi.length){updateLink();return;}
  const r=o.rssi[0],now=performance.now();       // with several devices connected only the first is used
  linkSeen=now;val.rssi=r;seen.rssi=now;
  rs.push(r);if(rs.length>9)rs.shift();
  const m=med(rs);val.dist=dAt(m,pln,ref||DEF_REF);seen.dist=now;
  if(rs.length>=5){if(val.dist>maxD)maxD=val.dist;if(!minR||m<minR)minR=m;}
  updateLink();
}
function updateLink(){
  const now=performance.now(),ok=rs.length&&linkSeen&&now-linkSeen<3000,u=useFt?'ft':'m';
  document.querySelectorAll('.du').forEach(e=>e.textContent=u);
  $('cs').textContent=ref?'Reference: '+Math.round(ref.rssi)+' dBm at '+fmtD(ref.d)+' '+u:'Not calibrated: using a rough default, so treat distance as a trend only.';
  $('drop').textContent=espDrop?'kart dropped '+espDrop+' CAN frames':'';
  $('dmax').textContent=maxD?fmtD(maxD):'--';
  $('wmin').textContent=minR?'weakest this session: '+Math.round(minR)+' dBm':'';
  const q=$('lq');
  if(!ok){$('dbm').textContent='--';$('dist').textContent='--';$('drng').textContent='';q.textContent='NO LINK DATA';q.className='pill';setBars(0,'');return;}
  const m=med(rs),rf=ref||DEF_REF;
  const lv=m>-55?['EXCELLENT',5,'ok','#2fd27a']:m>-67?['GOOD',4,'ok','#2fd27a']:m>-75?['FAIR',3,'mid','#ffd60a']:
    m>-82?['WEAK',2,'mid','#ff9f1a']:m>-88?['POOR',1,'','#ff3b3b']:['CRITICAL',1,'','#ff3b3b'];
  q.textContent=lv[0];q.className='pill '+lv[2];setBars(lv[1],lv[3]);
  $('dbm').textContent=Math.round(m);
  $('dist').textContent=fmtD(dAt(m,pln,rf));
  $('drng').textContent='likely '+fmtD(dAt(m,pln+0.5,rf))+' to '+fmtD(dAt(m,Math.max(1.5,pln-0.5),rf))+' '+u;
}
$('cu').value=useFt?'ft':'m';$('cn').value=pln;
$('cu').onchange=function(){useFt=this.value==='ft';save('kdu',this.value);updateLink();};
$('cn').oninput=function(){const v=parseFloat(this.value);if(v>=1.5&&v<=4){pln=v;save('kdn',v);maxD=0;updateLink();}};
$('calb').onclick=()=>{
  if(rs.length<5){alert('Wait a few seconds for signal readings first.');return;}
  const d=parseFloat($('cd').value);if(!(d>0))return;
  ref={rssi:med(rs),d:useFt?d*M_PER_FT:d};save('kdref',JSON.stringify(ref));maxD=0;updateLink();};
$('clr').onclick=()=>{ref=null;try{localStorage.removeItem('kdref');}catch(e){}maxD=0;updateLink();};
$('rst').onclick=()=>{maxD=0;minR=0;updateLink();};
updateLink();

// ---------- main loop ----------
setInterval(()=>{
  const now=performance.now();
  if(demo)demoStep();
  renderTach();renderBars(now);renderCards(now);
  if(rec)logRow();
  ALLKEYS.forEach(k=>hist[k][hp]=val[k]===undefined?NaN:val[k]);hp=(hp+1)%HN;
  drawGraph();
},50);
const st=$('st');
function status(){
  if(demo){st.textContent='DEMO';st.className='pill mid';}
  else if(!connected){st.textContent='OFFLINE';st.className='pill';}
  else if(fps===0){st.textContent='CONNECTED - NO CAN FRAMES';st.className='pill mid';}
  else{st.textContent='LIVE';st.className='pill ok';}
}
setInterval(()=>{fps=nfr-lastN;lastN=nfr;status();updateLink();
  $('info').textContent=fps+' fps'+(rec?'  |  '+rows.length+' rows'+(rawOn?', '+raw.length+' raw':''):'');},1000);

// ---------- connection ----------
function conn(){
  const w=new WebSocket('ws://'+(location.hostname||'192.168.4.1')+':81');w.binaryType='arraybuffer';
  w.onopen=()=>{connected=true;status();};
  w.onclose=()=>{connected=false;status();setTimeout(conn,1000);};
  w.onmessage=e=>{if(typeof e.data==='string'){try{onStatus(JSON.parse(e.data));}catch(x){}}else onFrames(e.data);};
}
conn();
</script></body></html>)KD";