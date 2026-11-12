# Stop a live guest thread at a Linux syscall and name the return addresses on its stack.
# Usage: livestack.py <pid> <tid> <linux syscall name | now> [words=260]
import sys,re,subprocess,bisect,os
pid,tid,sc=sys.argv[1],sys.argv[2],sys.argv[3]; words=int(sys.argv[4]) if len(sys.argv)>4 else 260
D='/home/zpt/projects/darling/build/destdir/usr/local/libexec/darling'
stop=[] if sc=='now' else ['-ex',f'catch syscall {sc}','-ex','continue']   # 'now': wherever the thread is
out=subprocess.run(['gdb','-q','-p',tid,'-batch','-ex','set debuginfod enabled off']+stop+['-ex',f'x/{words}gx $rsp'],capture_output=True,text=True,timeout=40).stdout
maps=[]
for l in open(f'/proc/{pid}/maps'):
    m=re.match(r'([0-9a-f]+)-([0-9a-f]+) (\S+) (\S+) \S+ \S+\s*(.*)',l)
    if 'x' in m[3] and m[5]: maps.append((int(m[1],16),int(m[2],16),int(m[4],16),m[5]))
cache={}
def syms(path):
    if path not in cache:
        f=path if os.path.exists(path) else D+path
        r=[]
        if os.path.exists(f) and '.so' not in os.path.basename(f):
            o=subprocess.run(['llvm-nm','-n','--arch=x86_64',f],capture_output=True,text=True).stdout
            for l in o.splitlines():
                p=l.split(None,2)
                if len(p)==3 and p[1] in 'tT': r.append((int(p[0],16),p[2]))
            t=subprocess.run(['llvm-otool','-arch','x86_64','-l',f],capture_output=True,text=True).stdout
            m=re.search(r'segname __TEXT\n\s+vmaddr (0x[0-9a-f]+)',t); base=int(m[1],16) if m else 0
        else: base=0
        cache[path]=(base,r)
    return cache[path]
def name(v):
    for a,b,off,path in maps:
        if a<=v<b:
            first=min(x[0] for x in maps if x[3]==path)
            base,r=syms(path); rel=v-first+base
            i=bisect.bisect_right([s[0] for s in r],rel)-1
            bn=os.path.basename(path)
            if i>=0 and rel-r[i][0]<0x4000: return f"{bn}`{r[i][1]}+{rel-r[i][0]:#x}"
            return f"{bn}+{v-first:#x}"
n=0
for l in out.splitlines():
    if not l.startswith('0x') or ':' not in l: continue
    for t in l.split(':',1)[1].split():
        v=int(t,16)
        if 0x7f0000000000<v<0x7fffff000000:
            x=name(v)
            if x and n<28: print('  ',x[:120]); n+=1
