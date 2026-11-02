import re,sys,os
src,out=sys.argv[1:3]
mods=[('aries_api','Core API: init, health, firmware, EEPROM, GPIO, PRBS test mode'),
('aries_link','Link status'),('aries_margin','Lane margining and eye measurement'),
('aries_misc','Per-lane diagnostics and helpers'),('aries_i2c','I2C transport and raw register access')]
doc=re.compile(r'/\*\*(.*?)\*/\s*\n((?:[A-Za-z_][^;{/]*?)\([^;{]*?\))\s*\{',re.S)
def clean(c):
    return [re.sub(r'^\s*\* ?','',l).rstrip() for l in c.split('\n')]
o=[open(sys.argv[3]).read().rstrip(),'']
notes={}
for b in open(sys.argv[4]).read().split('\n# ')[0:]:
    ls=b.lstrip('# ').strip().split('\n')
    notes[ls[0]]=ls[1:]
used=set()
total=0;miss=[]
for m,t in mods:
    h=open(f'{src}/{m}.h',errors='replace').read()
    c=open(f'{src}/{m}.c',errors='replace').read()
    declared=set(re.findall(r'\b([a-zA-Z_]\w*)\s*\(',h))
    o+=[f'## {t} (`{m}.h`)','']
    seen=set()
    for d,sig in doc.findall(c):
        n=re.search(r'([a-zA-Z_]\w*)\s*\($|([a-zA-Z_]\w*)\s*\(',sig)
        name=n.group(1) or n.group(2)
        if name not in declared or name in seen: continue
        seen.add(name);total+=1
        d=d.split('/**')[-1];lines=clean(d);desc=[];params=[];ret=None
        for l in lines:
            s=l.strip()
            p=re.match(r'@param(?:\[[^\]]*\])?\s+(\S+)\s*(.*)',s)
            if p: params.append([p.group(1),p.group(2)]);cur=params[-1];continue
            r=re.match(r'@return\s*(.*)',s)
            if r: ret=r.group(1);continue
            s=re.sub(r'^@brief\s*','',s)
            if params and ret is None and s: params[-1][1]+=' '+s
            elif s and ret is None: desc.append(s)
        o+=[f'### `{name}`','','```c',re.sub(r'\s+',' ',sig).strip()+';','```','',' '.join(desc),'']
        if params:
            o+=[f'- `{a}`: {b}'.rstrip() for a,b in params]+['']
        if ret: o+=[f'Returns: {ret}','']
    protos=set(re.findall(r'^(?:AriesErrorType|int|float|void|const char\*|uint8_t)\s+([a-z]\w+)\(',h,re.M))
    u=sorted(protos-seen); miss+= [(m,x) for x in u]
    if u:
        o+=['### Functions without SDK comments','','These descriptions were written from the source code and are not Astera\'s.','']
        hc=re.sub(r'/\*.*?\*/','',h,flags=re.S)
        for x in u:
            sig=re.search(r'^[A-Za-z][\w \t\n*]*?\b'+x+r'\s*\([^;]*\)',hc,re.M).group(0)
            sig=re.sub(r'\s+',' ',sig).strip()
            sp=[re.findall(r'(\w+)$',q.strip())[0] for q in sig[sig.index('(')+1:-1].split(',')]
            n=notes[x];used.add(x)
            np=[re.match(r'- (\w+): (.*)',l) for l in n if l.startswith('- ')]
            if [m.group(1) for m in np]!=sp: sys.exit(f'params differ for {x}: {sp}')
            r=[l[9:] for l in n if l.startswith('Returns: ')]
            o+=[f'#### `{x}`','','```c',sig+';','```','',n[0],'']
            o+=[f'- `{m.group(1)}`: {m.group(2)}' for m in np]+['']
            if r: o+=[f'Returns: {r[0]}','']
open(out,'w').write('\n'.join(o)+'\n')
if set(notes)!=used: sys.exit(f'unused notes: {set(notes)-used}')
print('documented',total);print('undocumented',len(miss));print(miss)
