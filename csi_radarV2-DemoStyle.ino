#include <WiFi.h>
#include <pgmspace.h>
#include <errno.h>
#include <fcntl.h>
#include <lwip/sockets.h>
#include <lwip/tcp.h>
#include "esp_wifi.h"
#include "ping/ping_sock.h"
#include "lwip/ip_addr.h"
#include "mbedtls/base64.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

const char* WIFI_SSID = "TP-LINK_AP_84C8";
const char* WIFI_PASS = "27421116";
const uint32_t PING_INTERVAL_MS = 20;
const uint32_t TICK_MS = 40;

const char INDEX_HTML[] PROGMEM = R"HTML(
<!doctype html>
<html lang="ar" dir="rtl">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover">
<title>CSI Radar</title>
<style>
:root{--bg:#f3f6f5;--card:#ffffff;--ink:#10201e;--mut:#667a76;--acc:#0b7d6c;--esp:#7c3aed;--ray:#1d5fd1;--red:#e5252a;--amb:#d97706;--ok:#16a34a;--okbg:rgba(22,163,74,.12);--stbg:rgba(217,119,6,.12);--ln:rgba(16,32,30,.09);--fill:rgba(16,32,30,.06);--fill2:rgba(16,32,30,.12)}
@media(prefers-color-scheme:dark){:root{--bg:#0a1211;--card:#111c1b;--ink:#e3efec;--mut:#86a09b;--acc:#35d6ae;--esp:#a78bfa;--ray:#6aa6ff;--red:#ff5252;--amb:#fbbf24;--ok:#4ade80;--okbg:rgba(74,222,128,.14);--stbg:rgba(251,191,36,.14);--ln:rgba(227,239,236,.1);--fill:rgba(227,239,236,.07);--fill2:rgba(227,239,236,.14)}}
[hidden]{display:none!important}
html,body{margin:0;height:100%;overflow:hidden;color:var(--ink);font:14px/1.5 "Segoe UI",Tahoma,system-ui,sans-serif}
body{background:radial-gradient(circle at 38% 46%,color-mix(in srgb,var(--card) 75%,var(--bg)),var(--bg) 72%)}
canvas{position:fixed;inset:0;width:100%;height:100%;touch-action:none;cursor:grab}
canvas:active{cursor:grabbing}
#lw{position:fixed;left:10px;right:10px;bottom:calc(10px + env(safe-area-inset-bottom,0px));max-height:38vh;display:flex;flex-direction:column;background:color-mix(in srgb,var(--card) 94%,transparent);border:1px solid var(--ln);border-radius:20px;box-shadow:0 10px 32px rgba(16,32,30,.08);overflow:hidden}
@media(min-width:900px){#lw{left:auto;right:8px;width:340px;top:8px;bottom:8px;max-height:none}}
#hd{display:flex;justify-content:space-between;align-items:center;padding:14px 16px 10px}
#hd h3{margin:0;font-size:16px;font-weight:700;display:flex;align-items:center;gap:8px}
#hd h3:before{content:"";width:9px;height:9px;border-radius:50%;background:var(--acc)}
#pps{font-size:12px;color:var(--mut);background:var(--fill);border-radius:99px;padding:2px 11px;font-variant-numeric:tabular-nums}
#ct{display:flex;flex-direction:column;gap:12px;padding:0 14px 12px}
.g{display:flex;flex-wrap:wrap;gap:6px;align-items:center}
.gt{flex-basis:100%;font-size:12px;color:var(--mut);margin-bottom:1px}
button{font:inherit;font-size:13px;color:var(--ink);background:var(--fill);border:0;border-radius:99px;padding:5px 13px;cursor:pointer;white-space:nowrap;transition:background .15s,color .15s,transform .1s}
button:hover{background:var(--fill2)}
button:active{transform:scale(.96)}
button.on{background:var(--acc);color:var(--card)}
button.pri{background:color-mix(in srgb,var(--acc) 16%,transparent);color:var(--acc);font-weight:600}
button.pri:hover{background:color-mix(in srgb,var(--acc) 26%,transparent)}
button.dg{color:var(--red)}
button:focus-visible,input:focus-visible{outline:2px solid var(--ray);outline-offset:2px}
#sc{margin:0 14px 10px;width:calc(100% - 28px);accent-color:var(--acc)}
#log{flex:1;min-height:0;overflow:auto;padding:10px;border-top:1px solid var(--ln);scrollbar-width:thin}
.e{display:flex;gap:10px;align-items:center;padding:7px 10px;border-radius:12px;margin-bottom:5px;color:var(--mut);font-size:13px;border-inline-start:3px solid transparent;background:var(--fill)}
.e span{flex:1}
.e time{font-variant-numeric:tabular-nums;opacity:.75;font-size:12px}
.e.ok{background:var(--okbg);border-inline-start-color:var(--ok);color:var(--ink);font-weight:600}
.e.still{background:var(--stbg);border-inline-start-color:var(--amb);color:var(--ink);font-weight:600}
.rb{padding:1px 10px;font-size:12px}
@media(max-width:899px){
 #ct{flex-direction:row;overflow-x:auto;gap:14px;scrollbar-width:none}
 .g{flex-wrap:nowrap;flex:none}
 .g+.g{padding-inline-start:14px;border-inline-start:1px solid var(--ln)}
 .gt{display:none}
}
</style>
</head>
<body>
<canvas id="m"></canvas>
<div id="lw">
<div id="hd"><h3>رادار CSI</h3><span id="pps">—</span></div>
<div id="ct">
<div class="g"><span class="gt">العرض</span><button id="bL">مباشر</button><button id="bR">▶ آخر حركة</button><button id="bP">⏯</button><button id="bS">×1</button><button id="bX">مسارات الانعكاس</button></div>
<div class="g"><span class="gt">الذكاء الاصطناعي</span><button id="bA">AI</button><button id="bT" class="pri">درّب الذكاء الاصطناعي</button><button id="bM">راجع الذاكرة</button><button id="bC" class="dg">امسح الذاكرة</button></div>
<div class="g"><span class="gt">المعايرة والبيانات</span><button id="bD">معايرة المسافة</button><button id="bE">تصدير الذاكرة</button><button id="bI">استيراد الذاكرة</button><button id="bV">تنزيل CSV</button></div>
</div>
<input type="file" id="fi" accept="application/json" hidden>
<input type="range" id="sc" hidden>
<div id="log"></div>
</div>
<script>
const cv=document.getElementById('m'),x=cv.getContext('2d'),dpr=Math.min(devicePixelRatio||1,2),$=s=>document.querySelector(s);
const C=3e8,Z=2.8,NF=21,KEY='csiAI2',CAP=15000,TICK=40;
let LAM=C/2.437e9,chn=0;
let W=8,D=5,w=0,h=0,F=1,CX=0,CY=0,PW=0,yaw=-.6,pit=.55,dist=12;
let emp=null,avg=null,fb=null,cal=0,sM=0,pd=0,nsM=.04,nsP=.04,thM=.1,thP=.1,st=0,qt=0,peak=0,t0=0,cand=0,cc=0,fc=0,bw=[],model=null,mlOn=false,tr=null,pOut=[1,0,0],rssiE=null,A0=-40;
let rssiBuf=[],sel=null,sb=[],sbT=[],sA=new Array(8).fill(0),sAn=0,slT=0,brF=0,slF=0,tvh=[],busy=false;
const wm=new Float64Array(64),wv=new Float64Array(64);
const ND=2.8,PREP=6,REC=10,SKIP=4,STEPS=[[0,"اطلع من المنطقة وخلّها فاضية"],[2,"قف ثابت داخل المنطقة بدون حركة"],[1,"تحرّك داخل المنطقة (مشي أو لوّح بيدك)"]];
let last=0,wasLive=false,cnt=0,manual=false,showRef=false,rp=null,lastEv=null,spd=1;
const RT=new Float64Array(CAP),RS=new Float32Array(CAP),RP=new Float32Array(CAP),RK=new Uint8Array(CAP),T0=performance.now();let RN=0;
const rmin=()=>Math.max(0,RN-CAP),rget=i=>{const k=i%CAP;return[RT[k],RS[k],RP[k],RK[k]]};
const P={ap:{x:1,y:2.5,z:1.4},esp:{x:6.5,y:2.9,z:.8}};
const CO={};
function colors(){['ink','mut','ray','acc','red','amb','esp','card'].forEach(n=>CO[n]=getComputedStyle(document.documentElement).getPropertyValue('--'+n).trim())}
colors();matchMedia('(prefers-color-scheme:dark)').addEventListener('change',colors);
const cr=(i,j,k)=>({x:i*W,y:j*D,z:k*Z});
const am=a=>a.indexOf(Math.max(a[0],a[1],a[2]));
function log(msg,kind,ev){
 const d=document.createElement('div');d.className='e '+(kind||'');
 const t=document.createElement('time'),s=document.createElement('span');
 t.textContent=new Date().toLocaleTimeString('en-GB');s.textContent=msg;d.append(t,s);
 if(ev){const b=document.createElement('button');b.className='rb';b.textContent='▶ إعادة';b.onclick=()=>replay(ev);d.append(b)}
 const l=$('#log');l.prepend(d);while(l.children.length>60)l.lastChild.remove()}
function fit(){w=innerWidth;h=innerHeight;cv.width=w*dpr;cv.height=h*dpr;x.setTransform(dpr,0,0,dpr,0,0);PW=w>=900?348:0;CX=(w-PW)/2;CY=h*(PW?.55:.42);F=Math.min((w-PW)*1.15,h*1.5)}
function pr(p){
 const X=p.x-W/2,Y=p.y-D/2,Q=p.z-Z/2,cy=Math.cos(yaw),sy=Math.sin(yaw),ce=Math.cos(pit),se=Math.sin(pit);
 const x1=X*cy-Y*sy,y1=X*sy+Y*cy,y2=y1*ce-Q*se,z2=y1*se+Q*ce,k=F/Math.max(y2+dist,.5);
 return[CX+x1*k,CY-z2*k,k]}
function unp(x1,y2,z2){
 const cy=Math.cos(yaw),sy=Math.sin(yaw),ce=Math.cos(pit),se=Math.sin(pit);
 const y1=y2*ce+z2*se,z1=-y2*se+z2*ce;
 return{x:x1*cy+y1*sy+W/2,y:-x1*sy+y1*cy+D/2,z:z1+Z/2}}
const len=(a,b)=>Math.hypot(a.x-b.x,a.y-b.y,a.z-b.z);
function paths(){
 const A=P.ap,B=P.esp,d=len(A,B)||.01,o=[];
 [['x',0],['x',W],['y',0],['y',D],['z',0],['z',Z]].forEach(([a,v])=>{
  const I={x:A.x,y:A.y,z:A.z};I[a]=2*v-A[a];
  const t=(v-I[a])/(B[a]-I[a]||1e-6),R={x:I.x+t*(B.x-I.x),y:I.y+t*(B.y-I.y),z:I.z+t*(B.z-I.z)};
  const L=len(I,B);o.push({pts:[A,R,B],L,g:-.7*d/L})});
 return o}
function along(p,dd){let a=p[0];for(let i=1;i<p.length;i++){const b=p[i],l=len(a,b);if(dd<=l){const f=dd/l;return{x:a.x+(b.x-a.x)*f,y:a.y+(b.y-a.y)*f,z:a.z+(b.z-a.z)*f}}dd-=l;a=b}return null}
const VAL=[];for(let j=0;j<64;j++){const k=j-32;if(k&&Math.abs(k)<=26)VAL.push(j)}
sel=VAL.slice();
const BAND=new Array(64).fill(-1);VAL.forEach((j,n)=>BAND[j]=Math.floor(n*8/52));
function bands(a){const s=new Array(8).fill(0),n=new Array(8).fill(0);for(let j=0;j<64;j++)if(BAND[j]>=0){s[BAND[j]]+=a[j];n[BAND[j]]++}return s.map((v,i)=>v/n[i])}
function dev(a,b){let s=0,n=0;for(const j of sel)if(b[j]>20){s+=Math.abs(a[j]-b[j])/b[j];n++}return n?s/n:0}
const hist=VAL.map(()=>[]);
function hampel(hh,v){
 hh.push(v);if(hh.length>7)hh.shift();if(hh.length<7)return v;
 const s=hh.slice().sort((p,q)=>p-q),med=s[3],mad=1.4826*hh.map(u=>Math.abs(u-med)).sort((p,q)=>p-q)[3];
 return mad>0&&Math.abs(v-med)>3*mad?med:v}
function push(){const k=RN%CAP;RT[k]=performance.now()-T0;RS[k]=sM;RP[k]=pd;RK[k]=st;RN++}
function layout(d){
 W=Math.min(18,Math.max(5,Math.ceil(d+3)));D=Math.min(10,Math.max(3.5,Math.round(W*.55)));
 P.ap.x=1;P.ap.y=D/2;P.esp.x=1+d;P.esp.y=D/2+.4;dist=Math.max(12,W*1.5)}
function finishCal(){
 const s=rssiBuf.slice().sort((p,q)=>p-q),rs=s[s.length>>1],d=Math.min(15,Math.max(1.5,Math.pow(10,(A0-rs)/(10*ND))));
 thM=Math.max(.05,nsM*2.5);thP=Math.max(.06,nsP*3);
 log('اكتملت المعايرة. RSSI '+rs.toFixed(0)+' dBm ← المسافة التقديرية ≈ '+d.toFixed(1)+' م (تقدير من RSSI وفيه خطأ كبير)');
 if(!manual)layout(d)}
function slowF(eb){
 const n=sb.length,fs=(n-1)*1000/(sbT[n-1]-sbT[0]);let br=0,sl=0;
 for(let k=0;k<8;k++){
  let mu=0;for(let i=0;i<n;i++)mu+=sb[i][k];mu/=n;
  let v=0;for(let i=0;i<n;i++){const t=sb[i][k]-mu;v+=t*t}v/=n;
  let pw=0;
  for(const f of[.2,.3,.4,.5]){const wd=2*Math.PI*f/fs;let re=0,im=0;for(let i=0;i<n;i++){const t=sb[i][k]-mu;re+=t*Math.cos(wd*i);im+=t*Math.sin(wd*i)}pw+=2*(re*re+im*im)/(n*n)}
  br+=pw/(v+1e-9);sl+=Math.sqrt(v)/eb[k]}
 return[br/8,sl/8]}
function feats(){
 const n=bw.length,mu=new Array(8).fill(0),sd=new Array(8).fill(0),eb=bands(emp);
 bw.forEach(b=>b.forEach((v,i)=>mu[i]+=v/n));
 bw.forEach(b=>b.forEach((v,i)=>sd[i]+=(v-mu[i])*(v-mu[i])/n));
 let tm=0,tv=0;tvh.forEach(v=>tm+=v/tvh.length);tvh.forEach(v=>tv+=(v-tm)*(v-tm)/tvh.length);
 return[...mu.map((v,i)=>v/eb[i]-1),...sd.map((v,i)=>Math.sqrt(v)/eb[i]),sM,pd,tv*1e4,brF,slF]}
function mlFactory(){
 const NF=21;
 const sm=z=>{const m=Math.max(z[0],z[1],z[2]),e=z.map(v=>Math.exp(v-m)),s=e[0]+e[1]+e[2];return e.map(v=>v/s)};
 const predict=(f,M)=>sm(M.w.map(r=>{let s=r[NF];for(let i=0;i<NF;i++)s+=r[i]*(f[i]-M.mu[i])/M.sd[i];return s}));
 function train(S){
  const N=S.length,mu=new Array(NF).fill(0),sd=new Array(NF).fill(0);
  S.forEach(([f])=>f.forEach((v,i)=>mu[i]+=v/N));
  S.forEach(([f])=>f.forEach((v,i)=>sd[i]+=(v-mu[i])*(v-mu[i])/N));
  const M={mu,sd:sd.map(v=>Math.max(Math.sqrt(v),1e-3)),w:[0,1,2].map(()=>new Array(NF+1).fill(0))};
  const X=S.map(([f])=>f.map((v,i)=>(v-M.mu[i])/M.sd[i]));
  for(let ep=0;ep<400;ep++){
   const g=[0,1,2].map(()=>new Array(NF+1).fill(0));
   X.forEach((q,n)=>{const p=sm(M.w.map(r=>{let s=r[NF];for(let i=0;i<NF;i++)s+=r[i]*q[i];return s}));
    for(let c=0;c<3;c++){const e=p[c]-(S[n][1]==c?1:0);for(let i=0;i<NF;i++)g[c][i]+=e*q[i]/N;g[c][NF]+=e/N}});
   for(let c=0;c<3;c++)for(let i=0;i<=NF;i++)M.w[c][i]-=.5*(g[c][i]+(i<NF?.001*M.w[c][i]:0))}
  return M}
 const acc=(M,S)=>S.length?S.filter(([f,l])=>{const p=predict(f,M);return p.indexOf(Math.max(p[0],p[1],p[2]))==l}).length/S.length:0;
 function cv(S){
  const ids=[...new Set(S.map(s=>s[2]))];
  if(ids.length>=2){let t=0;ids.forEach(id=>{t+=acc(train(S.filter(s=>s[2]!=id)),S.filter(s=>s[2]==id))});return{a:t/ids.length,k:ids.length}}
  const A=[],B=[];[0,1,2].forEach(c=>{const L=S.filter(s=>s[1]==c),k=Math.floor(L.length*.7);A.push(...L.slice(0,k));B.push(...L.slice(k+5))});
  return{a:B.length?acc(train(A),B):0,k:1}}
 return{train,acc,cv,predict}}
const ML=mlFactory(),jobs=new Map();let wk=null,jid=0;
const exec=(op,S)=>op=='train'?ML.train(S):ML.cv(S);
try{
 wk=new Worker(URL.createObjectURL(new Blob(['const ML=('+mlFactory.toString()+')();onmessage=e=>{const{id,op,S}=e.data;postMessage({id,r:op=="train"?ML.train(S):ML.cv(S)})}'],{type:'text/javascript'})));
 wk.onmessage=e=>{const j=jobs.get(e.data.id);jobs.delete(e.data.id);if(j)j[0](e.data.r)};
 wk.onerror=()=>{wk=null;jobs.forEach(j=>j[0](exec(j[1],j[2])));jobs.clear()}
}catch(e){wk=null}
const run=(op,S)=>new Promise(res=>{if(!wk){setTimeout(()=>res(exec(op,S)),20);return}const id=++jid;jobs.set(id,[res,op,S]);wk.postMessage({id,op,S})});
let mem=[];
const q4=(v,p)=>Math.round(v*p)/p,nSess=()=>new Set(mem.map(s=>s[2])).size;
const okM=M=>M&&Array.isArray(M.mu)&&M.mu.length==NF&&Array.isArray(M.sd)&&M.sd.length==NF&&Array.isArray(M.w)&&M.w.length==3&&M.w.every(r=>Array.isArray(r)&&r.length==NF+1&&r.every(Number.isFinite))&&M.mu.every(Number.isFinite)&&M.sd.every(Number.isFinite);
function ui(){$('#bT').textContent=mem.length?'درّب جلسة إضافية':'درّب الذكاء الاصطناعي'}
function pack(){return{v:2,nf:NF,m:{mu:model.mu.map(v=>q4(v,1e5)),sd:model.sd.map(v=>q4(v,1e5)),w:model.w.map(r=>r.map(v=>q4(v,1e5)))},S:mem.map(([f,l,s])=>[l,s,...f.map(v=>q4(v,1e4))])}}
function apply(j){
 if(!j||j.v!=2||j.nf!=NF||!okM(j.m)||!Array.isArray(j.S))return false;
 const S=j.S.filter(s=>Array.isArray(s)&&s.length==NF+2&&s.every(Number.isFinite)).map(s=>[s.slice(2),s[0],s[1]]);
 if(!S.length)return false;
 model=j.m;mem=S;mlOn=true;pOut=[1,0,0];ui();return true}
function saveAI(){
 try{localStorage.setItem(KEY,JSON.stringify(pack()));log('حُفظت الذاكرة في كاش المتصفح ('+mem.length+' عينة، '+nSess()+' جلسة)')}
 catch(e){log('فشل الحفظ في المتصفح (التخزين معطّل أو ممتلئ). استخدم "تصدير الذاكرة" كنسخة احتياطية')}}
function loadAI(){
 try{const raw=localStorage.getItem(KEY);if(!raw)return;
  if(apply(JSON.parse(raw)))log('حُمّلت ذاكرة الذكاء الاصطناعي من المتصفح: '+mem.length+' عينة من '+nSess()+' جلسة');
  else log('الذاكرة المحفوظة قديمة أو تالفة وتم تجاهلها')}
 catch(e){log('تعذّر قراءة الذاكرة المحفوظة')}}
function addMem(S){
 const nw=S.filter((_,i)=>i%2==0);mem=mem.concat(nw);
 [0,1,2].forEach(c=>{let L=mem.filter(s=>s[1]==c);
  while(L.length>200){const o=L.splice(Math.floor(Math.random()*(L.length-nw.length)),1)[0];mem.splice(mem.indexOf(o),1)}})}
function trStart(){
 if(busy){log('انتظر انتهاء التدريب الحالي');return}
 if(!emp||cal<100){log('انتظر اكتمال المعايرة أول');return}
 if(sb.length<100){log('انتظر ثواني قليلة حتى تمتلئ نافذة التحليل البطيء (~10 ث)');return}
 rp=null;$('#sc').hidden=true;tr={i:0,S:[],col:false,lab:0,t:performance.now(),prep:true,sid:Math.max(0,...mem.map(s=>s[2]))+1};
 log('بدأ التدريب: ٣ خطوات ('+PREP+' ثواني استعداد + '+REC+' ثواني تسجيل لكل خطوة)');log(STEPS[0][1])}
function trTick(now){
 if(!tr)return;const el=(now-tr.t)/1000;
 if(tr.prep){if(el>=PREP){tr.prep=false;tr.col=true;tr.lab=STEPS[tr.i][0];tr.t=now}}
 else if(el>=REC){tr.col=false;tr.i++;if(tr.i>=3){trFinish();return}tr.prep=true;tr.t=now;log(STEPS[tr.i][1])}}
async function trFinish(){
 const S=tr.S;tr=null;
 if([0,1,2].some(c=>S.filter(s=>s[1]==c).length<20)){log('بيانات التدريب ما كفت، أعد المحاولة');return}
 busy=true;
 if(model&&mem.length)log('مراجعة الذاكرة: النموذج السابق صنّف هذي الجلسة الجديدة بدقة '+Math.round(ML.acc(model,S)*100)+'%');
 addMem(S);log('جاري التدريب في الخلفية…');
 const M=await run('train',mem),r=await run('cv',mem);
 model=M;mlOn=true;pOut=[1,0,0];cand=st;cc=0;busy=false;ui();
 log('اكتمل التدريب: '+mem.length+' عينة من '+nSess()+' جلسة. دقة التحقق '+Math.round(r.a*100)+'% '+(r.k>1?'(leave-one-session-out، تقدير واقعي)':'(جلسة واحدة فقط — تقدير متفائل، أضف جلسة بوقت أو مكان مختلف)'));
 saveAI()}
async function reviewMem(){
 if(busy)return;if(!mem.length){log('الذاكرة فاضية');return}
 busy=true;log('جاري إعادة التدريب…');
 model=await run('train',mem);mlOn=true;pOut=[1,0,0];busy=false;
 log('راجعت الذاكرة وأعدت التدريب: '+[0,1,2].map(c=>['فاضي','حركة','ثابت'][c]+' '+mem.filter(s=>s[1]==c).length).join(' · ')+' — الدقة على الذاكرة '+Math.round(ML.acc(model,mem)*100)+'%');
 saveAI()}
function clearMem(){
 if(!confirm('تمسح ذاكرة الذكاء الاصطناعي نهائياً من المتصفح؟'))return;
 mem=[];model=null;mlOn=false;ui();
 try{localStorage.removeItem(KEY)}catch(e){}
 log('انمسحت ذاكرة الذكاء الاصطناعي')}
function download(name,type,text){const a=document.createElement('a');a.href=URL.createObjectURL(new Blob([text],{type}));a.download=name;a.click();setTimeout(()=>URL.revokeObjectURL(a.href),2000)}
function startMotion(){
 st=1;peak=sM;t0=performance.now();
 const ev={a:Math.max(rmin(),RN-50),b:null};lastEv=ev;
 log('حركة مكتشفة — شدة '+sM.toFixed(2),'ok',ev)}
function setState(n){
 const p=st;
 if(n==1){startMotion();return}
 if(p==1){lastEv.b=RN+8;const sec=((performance.now()-t0)/1000).toFixed(1);st=n;
  log(n==2?'الحركة وقفت بعد '+sec+' ث — شخص ثابت داخل المنطقة':'انتهت الحركة بعد '+sec+' ث — أعلى شدة '+peak.toFixed(2),n==2?'still':'')}
 else{st=n;log(n==2?'تغيّر ثابت في القناة — شخص أو شي ثابت داخل المنطقة':'المنطقة فاضية',n==2?'still':'')}}
function onCSI(v,r,t,n){
 const a=new Array(64).fill(0);
 VAL.forEach((j,k)=>{a[j]=hampel(hist[k],v[k])});
 cnt+=n;
 const now=performance.now();last=now;
 if(!wasLive){wasLive=true;log('وصلت أول بيانات CSI من الـ ESP32')}
 step(a,r,now,t)}
function step(a,r,now,ts){
 trTick(now);
 rssiE=rssiE==null?r:rssiE*.98+r*.02;
 if(!emp){emp=a.slice();avg=a.slice();fb=a.slice();cal=0;sM=0;pd=0;st=0;qt=cc=cand=0;bw=[];sb=[];sbT=[];sA.fill(0);sAn=0;tvh=[];rssiBuf=[];sel=VAL.slice();wm.fill(0);wv.fill(0);brF=slF=0;log('بدأت المعايرة… خلّ المنطقة فاضية وثابتة ~4 ثواني')}
 const m=dev(a,fb);
 for(let j=0;j<64;j++){avg[j]+=(a[j]-avg[j])*.1;fb[j]+=(a[j]-fb[j])*.05}
 sM=sM*.7+.3*m;
 const bn=bands(a);bw.push(bn);if(bw.length>25)bw.shift();
 let s1=0,s2=0;for(const j of sel){s1+=a[j];s2+=a[j]*a[j]}
 const mn=s1/sel.length;tvh.push(Math.sqrt(Math.max(s2/sel.length-mn*mn,0))/mn);if(tvh.length>25)tvh.shift();
 for(let k=0;k<8;k++)sA[k]+=bn[k];
 if(++sAn>=3){sb.push(sA.map(v=>v/sAn));sbT.push(ts);sA.fill(0);sAn=0;if(sb.length>100){sb.shift();sbT.shift()}}
 if(sb.length>=100&&now-slT>1000){slT=now;[brF,slF]=slowF(bands(emp))}
 if(cal<100){
  cal++;rssiBuf.push(r);
  for(let j=0;j<64;j++)emp[j]+=(a[j]-emp[j])/cal;
  if(cal<=40)for(const j of VAL){const dl=a[j]-wm[j];wm[j]+=dl/cal;wv[j]+=dl*(a[j]-wm[j])}
  if(cal==40){sel=VAL.map(j=>[j,wv[j]/39/(wm[j]*wm[j]+1e-9)]).sort((p,q)=>p[1]-q[1]).slice(0,39).map(p=>p[0]);log('اختيار الحزم الفرعية: استُبعدت 13 حزمة غير مستقرة من أصل 52')}
  pd=dev(avg,emp);
  if(cal>=40){nsM=cal==40?sM:nsM*.94+sM*.06;nsP=cal==40?pd:nsP*.94+pd*.06}
  if(cal==100)finishCal();
  push();return}
 pd=dev(avg,emp);
 if(bw.length>=25&&sb.length>=100&&++fc%2==0){
  const f=feats();
  if(tr){if(tr.col&&(now-tr.t)/1000>=SKIP)tr.S.push([f,tr.lab,tr.sid])}
  else if(mlOn&&model){const p=ML.predict(f,model);for(let c=0;c<3;c++)pOut[c]=pOut[c]*.6+.4*p[c]}}
 if(tr){push();return}
 if(st==0&&sM<thM&&pd<thP){
  nsM+=(sM-nsM)*.005;nsP+=(pd-nsP)*.005;thM=Math.max(.05,nsM*2.5);thP=Math.max(.06,nsP*3);
  if(++qt>125)for(let j=0;j<64;j++)emp[j]+=(avg[j]-emp[j])*.002}else qt=0;
 if(st==1)peak=Math.max(peak,sM);
 let raw;
 if(mlOn&&model){const c=am(pOut);raw=pOut[c]>.55?c:st}
 else raw=st==1?(sM>=thM*.8?1:pd>thP?2:0):sM>thM?1:st==2?(pd>thP*.7?2:0):(pd>thP*1.5?2:0);
 const need=raw==1?3:st==1?30:st==2?50:75;
 if(raw!=st){if(raw==cand){if(++cc>=need){cc=0;setState(raw)}}else{cand=raw;cc=1}}else{cand=st;cc=0}
 push()}
function replay(ev){
 if(!ev){log('ما فيه حركة مسجلة بعد');return}
 const a=Math.max(rmin(),ev.a),b=Math.min(RN-1,ev.b==null?RN-1:ev.b);
 if(b<=a){log('هذا التسجيل انمسح من الذاكرة (تُحفظ آخر ~'+Math.round(CAP*TICK/60000)+' دقايق)');return}
 rp={a,b,i:a,t:RT[a%CAP],play:true,sp:spd,last:performance.now()};
 const s=$('#sc');s.min=a;s.max=b;s.value=a;s.hidden=false}
function seg(a,b){const p=pr(a),q=pr(b);x.moveTo(p[0],p[1]);x.lineTo(q[0],q[1])}
function zone(n,fa,la,lw,c1,rings){
 const A=P.ap,B=P.esp,d=len(A,B)||.01,a=(d+n*LAM/2)/2,b=Math.sqrt(Math.max(a*a-d*d/4,0));
 const u={x:(B.x-A.x)/d,y:(B.y-A.y)/d,z:(B.z-A.z)/d},M={x:(A.x+B.x)/2,y:(A.y+B.y)/2,z:(A.z+B.z)/2};
 let v={x:u.y,y:-u.x,z:0};const vl=Math.hypot(v.x,v.y);if(vl<1e-3){v={x:1,y:0,z:0}}else{v.x/=vl;v.y/=vl}
 const q={x:u.y*v.z-u.z*v.y,y:u.z*v.x-u.x*v.z,z:u.x*v.y-u.y*v.x};
 const pt=(t,ph)=>{const c=Math.cos(t)*a,s=Math.sin(t)*b,cp=Math.cos(ph),sp=Math.sin(ph);return{x:M.x+u.x*c+(v.x*cp+q.x*sp)*s,y:M.y+u.y*c+(v.y*cp+q.y*sp)*s,z:M.z+u.z*c+(v.z*cp+q.z*sp)*s}};
 x.save();x.fillStyle=c1;x.strokeStyle=c1;
 if(fa>0){x.globalAlpha=fa;for(let k=1;k<=rings;k++){const t=Math.PI*k/(rings+1);x.beginPath();for(let i=0;i<=24;i++){const p=pr(pt(t,Math.PI*2*i/24));i?x.lineTo(p[0],p[1]):x.moveTo(p[0],p[1])}x.closePath();x.fill()}}
 x.globalAlpha=la;x.lineWidth=lw;x.beginPath();
 for(let k=0;k<6;k++){const ph=k*Math.PI/3;for(let i=0;i<=24;i++){const p=pr(pt(Math.PI*i/24,ph));i?x.lineTo(p[0],p[1]):x.moveTo(p[0],p[1])}}
 x.stroke();x.restore()}
function chart(red,amb,ink,card){
 const cw=Math.min(w-PW-24,440),ch=64,x0=12,y0=44;
 x.save();x.globalAlpha=.88;x.fillStyle=card;x.fillRect(x0,y0,cw,ch);x.globalAlpha=.3;x.strokeStyle=ink;x.strokeRect(x0+.5,y0+.5,cw,ch);x.restore();
 let a0,b0;if(rp){a0=rp.a;b0=rp.b}else{b0=RN-1;a0=Math.max(rmin(),b0-750)}
 const n=b0-a0;if(n<2)return;
 const ln=(A,th,c)=>{x.beginPath();for(let i=a0;i<=b0;i++){const X=x0+(i-a0)/n*cw,Y=y0+ch-Math.min(1,A[i%CAP]/(th*4))*ch;i==a0?x.moveTo(X,Y):x.lineTo(X,Y)}x.strokeStyle=c;x.lineWidth=1.5;x.stroke()};
 ln(RP,thP,amb);ln(RS,thM,red);
 x.save();x.globalAlpha=.5;x.setLineDash([4,4]);x.strokeStyle=ink;x.beginPath();x.moveTo(x0,y0+ch*.75);x.lineTo(x0+cw,y0+ch*.75);x.stroke();
 if(rp){const X=x0+(rp.i-a0)/n*cw;x.setLineDash([]);x.globalAlpha=.9;x.beginPath();x.moveTo(X,y0);x.lineTo(X,y0+ch);x.stroke()}
 x.restore()}
function draw(t){
 const now=performance.now();trTick(now);
 if(rp&&rp.play){rp.t+=(now-rp.last)*rp.sp;rp.last=now;while(rp.i<rp.b&&RT[(rp.i+1)%CAP]<=rp.t)rp.i++;if(rp.i>=rp.b)rp.play=false;$('#sc').value=rp.i}
 else if(rp)rp.last=now;
 x.clearRect(0,0,w,h);
 const{ink,mut,ray,acc,red,amb,esp,card}=CO;
 const live=wasLive&&now-last<2500;
 if(tr&&!live){tr=null;log('أُلغي التدريب بسبب انقطاع البيانات')}
 if(wasLive&&now-last>15000){if(lastEv&&lastEv.b==null)lastEv.b=RN;wasLive=false;emp=null;st=0;cal=0;log('انقطعت بيانات CSI')}
 const cur=rp?rget(rp.i):[0,sM,pd,st],vs=cur[1],vst=(live||rp)?cur[3]:0;
 const fl=[cr(0,0,0),cr(1,0,0),cr(1,1,0),cr(0,1,0)];
 x.beginPath();fl.forEach((p,j)=>{const r=pr(p);j?x.lineTo(r[0],r[1]):x.moveTo(r[0],r[1])});x.closePath();
 x.globalAlpha=.12;x.fillStyle=acc;x.fill();x.globalAlpha=.6;x.strokeStyle=ink;x.lineWidth=2;x.stroke();
 x.globalAlpha=.16;x.lineWidth=1;x.beginPath();
 for(let i=1;i<W;i++)seg({x:i,y:0,z:0},{x:i,y:D,z:0});for(let j=1;j<D;j++)seg({x:0,y:j,z:0},{x:W,y:j,z:0});x.stroke();x.globalAlpha=1;
 const A=P.ap,B=P.esp,pa=pr(A),pb=pr(B),dd=(t/1000*3)%(len(A,B)+2);
 if(vst==1){const I=Math.max(.35,Math.min(1,vs/(thM*3))),pl=.8+.4*Math.sin(t/110);zone(1,(.05+.1*I)*pl,.5+.5*I,1.5+3*I,red,12)}
 else if(vst==2)zone(1,.08,.7,2,amb,12);
 else zone(1,0,.35,1,mut,0);
 zone(3,0,.1,1,mut,0);
 x.save();x.strokeStyle=ray;x.globalAlpha=.7;x.lineWidth=2;x.setLineDash([8,6]);x.beginPath();x.moveTo(pa[0],pa[1]);x.lineTo(pb[0],pb[1]);x.stroke();x.restore();
 const q0=along([A,B],dd);if(q0){const r=pr(q0);x.fillStyle=ray;x.beginPath();x.arc(r[0],r[1],4,0,7);x.fill()}
 if(showRef){
  const ps=paths(),Lm=Math.max(...ps.map(p=>p.L)),cyc=Lm+2,d2=(t/1000*3)%cyc;
  for(let j=0;j<3;j++){const r=(t/1000*3+j*cyc/3)%cyc;x.globalAlpha=.3*(1-r/cyc);x.strokeStyle=ray;x.lineWidth=1.5;x.beginPath();x.arc(pa[0],pa[1],r*pa[2],0,7);x.stroke()}
  x.globalAlpha=1;
  ps.forEach(p=>{
   x.save();x.strokeStyle=ray;x.lineWidth=1.4;x.globalAlpha=Math.min(1,.25+Math.abs(p.g)*.7);x.setLineDash([6,5]);
   x.beginPath();p.pts.forEach((q,j)=>{const r=pr(q);j?x.lineTo(r[0],r[1]):x.moveTo(r[0],r[1])});x.stroke();x.restore();
   const q=d2<p.L&&along(p.pts,d2);if(q){const r=pr(q);x.fillStyle=ray;x.beginPath();x.arc(r[0],r[1],4,0,7);x.fill()}
   const R=p.pts[1],r=pr(R),l1=len(A,R);
   x.fillStyle=amb;x.globalAlpha=.35;x.beginPath();x.arc(r[0],r[1],.14*r[2],0,7);x.fill();
   x.globalAlpha=1;x.strokeStyle=amb;x.lineWidth=2;x.beginPath();x.arc(r[0],r[1],.14*r[2]+3,0,7);x.stroke();
   const e=d2-l1;if(e>0&&e<1){x.globalAlpha=1-e;x.beginPath();x.arc(r[0],r[1],(.14+e*.6)*r[2],0,7);x.stroke();x.globalAlpha=1}})}
 [['ap','الراوتر'],['esp','ESP32-S3']].forEach(([k,n])=>{
  const p=pr(P[k]),f=pr({x:P[k].x,y:P[k].y,z:0});
  x.save();x.strokeStyle=mut;x.setLineDash([3,4]);x.beginPath();x.moveTo(p[0],p[1]);x.lineTo(f[0],f[1]);x.stroke();x.restore();
  x.globalAlpha=.35;x.fillStyle=ink;x.beginPath();x.ellipse(f[0],f[1],8,4,0,0,7);x.fill();x.globalAlpha=1;
  x.fillStyle=k=='ap'?acc:esp;x.beginPath();if(k=='ap')x.rect(p[0]-9,p[1]-9,18,18);else x.arc(p[0],p[1],10,0,7);x.fill();
  x.fillStyle=ink;x.font='13px "Segoe UI",Tahoma,sans-serif';x.textAlign='center';x.fillText(n,p[0],p[1]-16)});
 const pm=pr({x:(A.x+B.x)/2,y:(A.y+B.y)/2,z:(A.z+B.z)/2});
 x.fillStyle=mut;x.font='13px "Segoe UI",Tahoma,sans-serif';x.textAlign='center';x.fillText('≈ '+len(A,B).toFixed(1)+' م (تقدير من RSSI)',pm[0],pm[1]+20);
 const calOn=!rp&&emp&&cal<100;
 x.font='bold 17px "Segoe UI",Tahoma,sans-serif';x.textAlign='center';
 let lbl=!rp&&!live?'بانتظار بيانات CSI…':calOn?'معايرة…':['هادئ','حركة مكتشفة','شخص ثابت'][vst];
 let fs=(!rp&&!live)||calOn?mut:[acc,red,amb][vst];
 if(mlOn&&!rp&&live&&!calOn&&!tr)lbl+=' · AI '+Math.round(Math.max(pOut[0],pOut[1],pOut[2])*100)+'%';
 if(tr){lbl='تدريب '+(tr.i+1)+'/3 — '+(tr.prep?'استعد: ':'يسجّل الآن: ')+STEPS[tr.i][1]+' ('+Math.max(0,Math.ceil((tr.prep?PREP:REC)-(now-tr.t)/1000))+')';fs=amb}
 x.fillStyle=fs;x.fillText((rp?'إعادة ▶  ':'')+lbl,CX,28,w-PW-24);
 chart(red,amb,ink,card);
 $('#bL').classList.toggle('on',!rp);$('#bA').classList.toggle('on',mlOn);
 requestAnimationFrame(draw)}
const ptrs=new Map();let drag=null,pdi=0,lx=0,ly=0;
const pdist=()=>{const v=[...ptrs.values()];return Math.hypot(v[0][0]-v[1][0],v[0][1]-v[1][1])};
cv.onpointerdown=e=>{cv.setPointerCapture(e.pointerId);ptrs.set(e.pointerId,[e.clientX,e.clientY]);lx=e.clientX;ly=e.clientY;
 if(ptrs.size==1){let best=null,bd=30;for(const k of['ap','esp']){const p=pr(P[k]),dx=Math.hypot(p[0]-e.clientX,p[1]-e.clientY);if(dx<bd){bd=dx;best=k}}drag=best}else{drag=null;pdi=pdist()}};
cv.onpointermove=e=>{if(!ptrs.has(e.pointerId))return;ptrs.set(e.pointerId,[e.clientX,e.clientY]);
 if(ptrs.size>=2){const n=pdist();if(pdi)dist=Math.min(40,Math.max(5,dist*pdi/n));pdi=n;return}
 if(drag){const dx=(e.clientX-CX)/F,dz=(CY-e.clientY)/F,O0=unp(dx,1-dist,dz),O1=unp(dx*20,20-dist,dz*20),s=(P[drag].z-O0.z)/(O1.z-O0.z||1e-6);
  P[drag].x=Math.min(W-.2,Math.max(.2,O0.x+s*(O1.x-O0.x)));P[drag].y=Math.min(D-.2,Math.max(.2,O0.y+s*(O1.y-O0.y)))}
 else{yaw+=(e.clientX-lx)*.008;pit=Math.min(1.45,Math.max(.1,pit+(e.clientY-ly)*.006))}
 lx=e.clientX;ly=e.clientY};
cv.onpointerup=cv.onpointercancel=e=>{ptrs.delete(e.pointerId);if(drag){drag=null;manual=true}pdi=0};
cv.onwheel=e=>{dist=Math.min(40,Math.max(5,dist*(1+e.deltaY*.001)))};
cv.ondblclick=()=>{if(confirm('تعيد معايرة المنطقة الفاضية من الصفر؟'))emp=null};
$('#bR').onclick=()=>replay(lastEv);
$('#bP').onclick=()=>{if(!rp){replay(lastEv);return}if(rp.play)rp.play=false;else{if(rp.i>=rp.b){rp.i=rp.a;rp.t=RT[rp.a%CAP]}rp.play=true;rp.last=performance.now()}};
$('#bS').onclick=e=>{const L=[.5,1,2,4];spd=L[(L.indexOf(spd)+1)%4];e.target.textContent='×'+spd;if(rp)rp.sp=spd};
$('#bL').onclick=()=>{rp=null;$('#sc').hidden=true};
$('#bX').onclick=e=>{showRef=!showRef;e.target.classList.toggle('on',showRef)};
$('#bT').onclick=trStart;$('#bM').onclick=reviewMem;$('#bC').onclick=clearMem;
$('#bA').onclick=()=>{if(!model){log('ما فيه نموذج مدرّب. اضغط "درّب الذكاء الاصطناعي"');return}mlOn=!mlOn;pOut=[1,0,0];cand=st;cc=0;log(mlOn?'الذكاء الاصطناعي: مفعّل':'الذكاء الاصطناعي: متوقف (عتبات ثابتة)')};
$('#bD').onclick=()=>{if(rssiE==null){log('ما فيه بيانات بعد');return}
 const v=parseFloat(prompt('كم المسافة الحقيقية بين الراوتر والـ ESP32 بالمتر؟ (قِسها بشريط قياس)'));
 if(v>0){A0=rssiE+10*ND*Math.log10(v);try{localStorage.setItem('csiA0',A0)}catch(e){}manual=false;layout(v);log('تمت معايرة المسافة: قوة الإشارة عند 1 م = '+A0.toFixed(0)+' dBm')}};
$('#bE').onclick=()=>{if(!model){log('ما فيه ذاكرة للتصدير');return}download('csi-memory.json','application/json',JSON.stringify(pack()));log('صُدّرت الذاكرة كملف JSON')};
$('#bI').onclick=()=>$('#fi').click();
$('#fi').onchange=e=>{const f=e.target.files[0];e.target.value='';if(!f)return;
 f.text().then(t=>{if(apply(JSON.parse(t))){log('استُوردت الذاكرة: '+mem.length+' عينة من '+nSess()+' جلسة');saveAI()}else log('الملف غير صالح أو من نسخة قديمة')}).catch(()=>log('تعذّر قراءة الملف'))};
$('#bV').onclick=()=>{const a=rmin(),rows=['t_ms,motion,drift,state'];for(let i=a;i<RN;i++){const k=i%CAP;rows.push(RT[k].toFixed(0)+','+RS[k].toFixed(4)+','+RP[k].toFixed(4)+','+RK[k])}download('csi-log.csv','text/csv',rows.join('\n'));log('نُزّل السجل: '+(RN-a)+' صف')};
$('#sc').oninput=e=>{if(!rp)return;rp.i=+e.target.value;rp.t=RT[rp.i%CAP];rp.play=false};
try{const a0=localStorage.getItem('csiA0');if(a0)A0=+a0}catch(e){}
loadAI();
addEventListener('resize',fit);fit();
setInterval(()=>{$('#pps').textContent=cnt+' إطار/ث';cnt=0},1000);
log('الصفحة جاهزة. بانتظار الـ ESP32…');
let seq=0;
function handle(j){
 const s=atob(j.b),v=new Array(52);
 for(let i=0;i<52;i++)v[i]=(s.charCodeAt(2*i)|(s.charCodeAt(2*i+1)<<8))/10;
 if(j.c&&j.c!==chn){chn=j.c;LAM=C/((j.c>=14?2484:2407+5*j.c)*1e6)}
 onCSI(v,j.r,j.t,j.n)}
async function poll(){
 const ac=new AbortController(),to=setTimeout(()=>ac.abort(),1500);
 try{
  const r=await fetch('/d?s='+seq,{signal:ac.signal,cache:'no-store'});
  const a=await r.json();
  clearTimeout(to);
  if(a.length&&a[a.length-1].q<seq)seq=a[0].q-1;
  for(const j of a){
   if(j.q<=seq)continue;
   seq=j.q;
   try{handle(j)}catch(_){}
  }
  setTimeout(poll,0);
 }catch(e){
  clearTimeout(to);
  setTimeout(poll,150);
 }}
poll();
requestAnimationFrame(draw);
</script>
</body>
</html>
)HTML";

const size_t PAGE_LEN = sizeof(INDEX_HTML) - 1;

struct Frame { int8_t d[128]; int8_t rssi; uint8_t ch; };
const uint32_t QN = 128;
Frame q[QN];
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
volatile uint32_t qh = 0, qtl = 0, dropped = 0, csiCount = 0;
uint8_t apMac[6];
volatile bool macOk = false;
esp_ping_handle_t pingH = NULL;
volatile bool needRestart = false;
int idxMap[52];

struct Msg {
  uint32_t seq;
  uint16_t len;
  char d[240];
};
const int RING = 16;
const int BATCH = 6;
Msg ring[RING];
uint32_t gseq = 0;

struct Cl {
  int fd;
  uint8_t st;
  uint32_t t0, tw, tl, seq;
  size_t off, len, po, rl;
  char buf[1700];
  char rq[256];
};
const int MAXC = 6;
Cl cl[MAXC];
char body[1500];
volatile int nCl = 0;
volatile uint32_t polls = 0;

static void csi_cb(void* ctx, wifi_csi_info_t* info) {
  if (!info || !info->buf || info->len < 128) return;
  if (macOk && memcmp(info->mac, apMac, 6) != 0) return;
  portENTER_CRITICAL(&mux);
  Frame& f = q[qh % QN];
  memcpy(f.d, info->buf, 128);
  f.rssi = info->rx_ctrl.rssi;
  f.ch = info->rx_ctrl.channel;
  qh++;
  if (qh - qtl > QN) { qtl = qh - QN; dropped++; }
  csiCount++;
  portEXIT_CRITICAL(&mux);
}

static bool popFrame(Frame& out) {
  bool ok = false;
  portENTER_CRITICAL(&mux);
  if (qtl != qh) { out = q[qtl % QN]; qtl++; ok = true; }
  portEXIT_CRITICAL(&mux);
  return ok;
}

static void startPing() {
  if (pingH) { esp_ping_stop(pingH); esp_ping_delete_session(pingH); pingH = NULL; }
  IPAddress g = WiFi.gatewayIP();
  ip_addr_t target;
  IP_ADDR4(&target, g[0], g[1], g[2], g[3]);

  esp_ping_config_t pc = ESP_PING_DEFAULT_CONFIG();
  pc.target_addr = target;
  pc.count = ESP_PING_COUNT_INFINITE;
  pc.interval_ms = PING_INTERVAL_MS;
  pc.timeout_ms = 50;
  pc.data_size = 16;
  pc.task_stack_size = 4096;

  esp_ping_callbacks_t cbs;
  memset(&cbs, 0, sizeof(cbs));
  if (esp_ping_new_session(&pc, &cbs, &pingH) == ESP_OK) esp_ping_start(pingH);
}

static void startCSI() {
  const uint8_t* b = WiFi.BSSID();
  bool nz = false;
  if (b) {
    memcpy(apMac, b, 6);
    for (int i = 0; i < 6; i++) if (apMac[i]) nz = true;
  }
  macOk = nz;

  wifi_csi_config_t cfg;
  memset(&cfg, 0, sizeof(cfg));
  cfg.lltf_en = true;
  cfg.htltf_en = false;
  cfg.stbc_htltf2_en = false;
  cfg.ltf_merge_en = true;
  cfg.channel_filter_en = true;
  cfg.manu_scale = true;
  cfg.shift = true;

  esp_wifi_set_csi_config(&cfg);
  esp_wifi_set_csi_rx_cb(csi_cb, NULL);
  esp_wifi_set_csi(true);
}

static void onWifi(arduino_event_id_t ev, arduino_event_info_t info) {
  if (ev == ARDUINO_EVENT_WIFI_STA_GOT_IP) needRestart = true;
}

static void dropC(Cl& c) {
  if (c.fd >= 0) close(c.fd);
  c.fd = -1;
  c.st = 0;
  c.off = c.len = c.po = c.rl = 0;
}

static bool again() {
  return errno == EAGAIN || errno == EWOULDBLOCK || errno == ENOMEM || errno == EINTR;
}

static bool tx(Cl& c) {
  if (c.len <= c.off) return true;
  int r = send(c.fd, c.buf + c.off, c.len - c.off, MSG_DONTWAIT);
  if (r > 0) {
    c.off += r;
    c.tw = millis();
    if (c.off >= c.len) { c.off = c.len = 0; c.tl = c.tw; }
    return true;
  }
  return r < 0 && again();
}

static void buildPoll(Cl& c) {
  uint32_t s = c.seq;
  if (s > gseq) s = 0;
  uint32_t first = s + 1;
  if (gseq >= first && gseq - first + 1 > (uint32_t)BATCH) first = gseq - BATCH + 1;
  size_t n = 0;
  body[n++] = '[';
  bool any = false;
  for (uint32_t k = first; k <= gseq && gseq > 0; k++) {
    Msg& m = ring[k % RING];
    if (m.seq != k) continue;
    if (n + m.len + 2 >= sizeof(body)) break;
    if (any) body[n++] = ',';
    memcpy(body + n, m.d, m.len);
    n += m.len;
    any = true;
  }
  body[n++] = ']';
  int h = snprintf(c.buf, sizeof(c.buf),
    "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: %u\r\nCache-Control: no-store\r\nConnection: keep-alive\r\n\r\n",
    (unsigned)n);
  memcpy(c.buf + h, body, n);
  c.len = h + n;
  c.off = 0;
  c.tw = millis();
  polls++;
}

static void route(Cl& c, uint32_t now) {
  c.off = 0;
  c.len = 0;
  c.tw = now;
  c.tl = now;
  c.t0 = now;
  c.po = 0;
  if (!strncmp(c.rq, "GET /d", 6)) {
    const char* p = strstr(c.rq, "s=");
    c.seq = p ? (uint32_t)strtoul(p + 2, NULL, 10) : 0;
    c.st = 5;
  } else if (!strncmp(c.rq, "GET / ", 6) || !strncmp(c.rq, "GET /?", 6) || !strncmp(c.rq, "GET /index", 10)) {
    c.len = snprintf(c.buf, sizeof(c.buf),
      "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nContent-Length: %u\r\nCache-Control: no-cache\r\nConnection: close\r\n\r\n",
      (unsigned)PAGE_LEN);
    c.st = 2;
  } else {
    c.len = snprintf(c.buf, sizeof(c.buf),
      "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
    c.st = 4;
  }
}

static void produce() {
  float acc[52];
  memset(acc, 0, sizeof(acc));
  int cnt = 0, rs = 0;
  uint8_t ch = 0;
  Frame f;
  int guard = 0;
  while (guard++ < (int)QN && popFrame(f)) {
    float am[52];
    float m0 = 0;
    for (int n = 0; n < 52; n++) {
      int i = idxMap[n];
      float re = f.d[2 * i], im = f.d[2 * i + 1];
      am[n] = sqrtf(re * re + im * im);
      m0 += am[n];
    }
    m0 /= 52.0f;
    if (m0 <= 0) continue;
    float s = 100.0f / m0;
    for (int n = 0; n < 52; n++) acc[n] += am[n] * s;
    rs += f.rssi;
    ch = f.ch;
    cnt++;
  }
  if (!cnt) return;
  uint8_t raw[104];
  char b64[160];
  float inv = 10.0f / cnt;
  for (int k = 0; k < 52; k++) {
    float v = acc[k] * inv;
    if (v > 65535) v = 65535;
    uint16_t u = (uint16_t)v;
    raw[2 * k] = u & 0xFF;
    raw[2 * k + 1] = u >> 8;
  }
  size_t ol = 0;
  mbedtls_base64_encode((unsigned char*)b64, sizeof(b64) - 1, &ol, raw, 104);
  b64[ol] = 0;
  uint32_t sq = gseq + 1;
  Msg& m = ring[sq % RING];
  int n = snprintf(m.d, sizeof(m.d), "{\"q\":%u,\"t\":%u,\"r\":%d,\"c\":%u,\"n\":%d,\"b\":\"%s\"}",
                   (unsigned)sq, (unsigned)millis(), rs / cnt, (unsigned)ch, cnt, b64);
  if (n > 0 && n < (int)sizeof(m.d)) {
    m.len = n;
    m.seq = sq;
    gseq = sq;
  }
}

static void serverTask(void*) {
  int ls = socket(AF_INET, SOCK_STREAM, 0);
  int on = 1;
  setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
  struct sockaddr_in a;
  memset(&a, 0, sizeof(a));
  a.sin_family = AF_INET;
  a.sin_port = htons(80);
  a.sin_addr.s_addr = htonl(INADDR_ANY);
  bind(ls, (struct sockaddr*)&a, sizeof(a));
  listen(ls, 6);
  fcntl(ls, F_SETFL, fcntl(ls, F_GETFL, 0) | O_NONBLOCK);

  uint32_t nextTick = millis() + TICK_MS;

  for (;;) {
    uint32_t now = millis();

    fd_set rs;
    FD_ZERO(&rs);
    FD_SET(ls, &rs);
    int mx = ls;
    for (int i = 0; i < MAXC; i++) {
      Cl& c = cl[i];
      if (c.fd >= 0 && (c.st == 1 || c.st == 5)) {
        FD_SET(c.fd, &rs);
        if (c.fd > mx) mx = c.fd;
      }
    }
    int32_t dt = (int32_t)(nextTick - now);
    if (dt < 1) dt = 1;
    if (dt > 4) dt = 4;
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = dt * 1000;
    select(mx + 1, &rs, NULL, NULL, &tv);
    now = millis();

    if (FD_ISSET(ls, &rs)) {
      for (int k = 0; k < 3; k++) {
        int fd = accept(ls, NULL, NULL);
        if (fd < 0) break;
        int s = -1;
        for (int i = 0; i < MAXC; i++) if (cl[i].fd < 0) { s = i; break; }
        if (s < 0) {
          uint32_t best = 0;
          for (int i = 0; i < MAXC; i++) {
            if (cl[i].st == 2) continue;
            uint32_t age = now - cl[i].t0;
            if (s < 0 || age > best) { best = age; s = i; }
          }
          if (s >= 0) dropC(cl[s]);
        }
        if (s < 0) { close(fd); continue; }
        fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
        int nd = 1;
        setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &nd, sizeof(nd));
        Cl& c = cl[s];
        c.fd = fd;
        c.st = 1;
        c.t0 = now;
        c.tw = now;
        c.tl = now;
        c.off = c.len = c.po = c.rl = 0;
      }
    }

    if ((int32_t)(now - nextTick) >= 0) {
      nextTick += TICK_MS;
      if ((int32_t)(now - nextTick) > (int32_t)TICK_MS) nextTick = now + TICK_MS;
      produce();
    }

    int cc = 0;
    for (int i = 0; i < MAXC; i++) {
      Cl& c = cl[i];
      if (c.fd < 0) continue;
      cc++;

      if (c.st == 1) {
        if (FD_ISSET(c.fd, &rs)) {
          int r = recv(c.fd, c.rq + c.rl, sizeof(c.rq) - 1 - c.rl, MSG_DONTWAIT);
          if (r == 0 || (r < 0 && !again())) { dropC(c); continue; }
          if (r > 0) {
            c.rl += r;
            c.rq[c.rl] = 0;
            if (strstr(c.rq, "\r\n\r\n") || c.rl >= sizeof(c.rq) - 1) route(c, now);
          }
        }
        if (c.st == 1 && now - c.t0 > (c.rl ? 4000u : 15000u)) { dropC(c); continue; }
      }

      if (c.st == 5) {
        if (FD_ISSET(c.fd, &rs)) {
          char tmp[64];
          int r = recv(c.fd, tmp, sizeof(tmp), MSG_DONTWAIT);
          if (r == 0 || (r < 0 && !again())) { dropC(c); continue; }
        }
        if (c.seq != gseq || now - c.tl >= 120) {
          buildPoll(c);
          c.st = 6;
          if (!tx(c)) { dropC(c); continue; }
        }
      }

      if (c.st == 6) {
        if (c.len && !tx(c)) { dropC(c); continue; }
        if (c.len == 0) {
          c.st = 1;
          c.rl = 0;
          c.t0 = now;
        } else if (now - c.tw > 2000) { dropC(c); continue; }
      } else if (c.st == 2) {
        if (c.len && !tx(c)) { dropC(c); continue; }
        if (c.len == 0) {
          if (c.po < PAGE_LEN) {
            size_t n = PAGE_LEN - c.po;
            if (n > 4096) n = 4096;
            int r = send(c.fd, INDEX_HTML + c.po, n, MSG_DONTWAIT);
            if (r > 0) { c.po += r; c.tw = now; }
            else if (r < 0 && !again()) { dropC(c); continue; }
          } else { dropC(c); continue; }
        }
        if (now - c.tw > 5000) { dropC(c); continue; }
      } else if (c.st == 4) {
        if (!tx(c) || c.len == 0) { dropC(c); continue; }
        if (now - c.tw > 3000) { dropC(c); continue; }
      }
    }
    nCl = cc;
  }
}

void setup() {
  setCpuFrequencyMhz(240);
  Serial.begin(115200);
  delay(500);

  int p = 0;
  for (int k = -26; k <= 26; k++) if (k) idxMap[p++] = (k + 64) % 64;

  for (int i = 0; i < RING; i++) { ring[i].seq = 0; ring[i].len = 0; }
  for (int i = 0; i < MAXC; i++) { cl[i].fd = -1; cl[i].st = 0; cl[i].off = cl[i].len = 0; }

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  esp_wifi_set_ps(WIFI_PS_NONE);
  WiFi.setAutoReconnect(true);
  WiFi.onEvent(onWifi);
  esp_wifi_set_bandwidth(WIFI_IF_STA, WIFI_BW_HT20);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
    if (millis() - t0 > 30000) ESP.restart();
  }
  Serial.println();
  Serial.print("Open: http://");
  Serial.println(WiFi.localIP());

  startCSI();
  startPing();
  needRestart = false;

  xTaskCreatePinnedToCore(serverTask, "srv", 8192, NULL, 4, NULL, 1);
}

void loop() {
  if (needRestart && WiFi.status() == WL_CONNECTED) {
    needRestart = false;
    startCSI();
    startPing();
    Serial.println("Reconnected: CSI + ping restarted");
  }

  static uint32_t lp = 0;
  static uint32_t lpoll = 0;
  if (millis() - lp >= 2000) {
    lp = millis();
    Serial.printf("CSI frames: %u dropped: %u clients: %d polls/s: %u\n", (unsigned)csiCount, (unsigned)dropped, (int)nCl, (unsigned)((polls - lpoll) / 2));
    lpoll = polls;
  }

  delay(50);
}
