import re,sys,os
src,out=sys.argv[1:3]
mods=[('aries_api','Core API: init, health, firmware, EEPROM, GPIO, PRBS test mode'),
('aries_link','Link status'),('aries_margin','Lane margining and eye measurement'),
('aries_misc','Per-lane diagnostics and helpers'),('aries_i2c','I2C transport and raw register access')]
doc=re.compile(r'/\*\*(.*?)\*/\s*\n((?:[A-Za-z_][^;{/]*?)\([^;{]*?\))\s*\{',re.S)
def clean(c):
    return [re.sub(r'^\s*\* ?','',l).rstrip() for l in c.split('\n')]
o=[open(sys.argv[3]).read().rstrip(),'']
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
        o+=['### Declared without a doc comment','']+[f'- `{x}`' for x in u]+['']
open(out,'w').write('\n'.join(o)+'\n')
print('documented',total);print('undocumented',len(miss));print(miss)
