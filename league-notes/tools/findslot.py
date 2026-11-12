import corelib,sys,struct,mmap
c=corelib.Core(sys.argv[1]); pat=bytes.fromhex(sys.argv[2])
f=open(sys.argv[1],'rb'); mm=mmap.mmap(f.fileno(),0,prot=mmap.PROT_READ)
def asc(b): return ''.join(chr(x) if 32<=x<127 else '.' for x in b)
def scan(p):
    out=[]
    for v,fsz,off in c.loads:
        i=mm.find(p,off,off+fsz)
        while i!=-1:
            out.append(v+i-off); i=mm.find(p,i+1,off+fsz)
    return out
hits=scan(pat); print("pattern at:",[hex(h) for h in hits][:10])
for h in hits[:4]:
    b=c.read(h-32,96)
    for i in range(0,96,32): print(f"   {h-32+i:#x}: {b[i:i+32].hex(' ',8)} {asc(b[i:i+32])}")
    refs=scan(struct.pack('<Q',h)); print("   pointers to it:",len(refs))
    for r in refs[:12]:
        m=c.mapping(r); b=c.read(r-16,48)
        print(f"     at {r:#x} [{(m[3] if m else 'anon')[-40:]}] ctx {b.hex(' ',8)}")
