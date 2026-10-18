import pickle, numpy as np
from skeleton import D, keys, rel, joint, rigid_err, allposes
E,RG,names=pickle.load(open('joints.pkl','rb'))
n=len(keys)
# 1. rigid groups
grp=list(range(n))
def find(a):
    while grp[a]!=a: a=grp[a]
    return a
for a in range(n):
    for b in range(n):
        if a!=b and RG[a,b]<0.01: grp[find(a)]=find(b)
groups={}
for a in range(n): groups.setdefault(find(a),[]).append(a)
G=list(groups.values())
print(len(G),'bones')
# world-space rest geometry per part
rest=D['rest']; objs=D['objs']
def verts_world(i):
    o=objs[keys[i]]; V=np.array([v[:3] for v in o['verts']]); return (np.c_[V,np.ones(len(V))]@rest[keys[i]])[:,:3]
GV=[np.vstack([verts_world(i) for i in g]) for g in G]
def gjoint(cg,pg):
    # use representative parts (first) of each group
    x,err,cnt=joint(G[cg][0],G[pg][0])
    pw=(np.append(x,1)@rest[keys[G[cg][0]]])[:3]         # pivot in rest world space
    dp=np.min(np.linalg.norm(GV[pg]-pw,axis=1)); dc=np.min(np.linalg.norm(GV[cg]-pw,axis=1))
    return pw,err,dp,dc
# root: group containing the part with most vertices near the centroid of the whole model -> choose pelvis = group of part 15 if present
allv=np.vstack(GV); centre=allv.mean(0)
root=min(range(len(G)),key=lambda g: np.linalg.norm(GV[g].mean(0)-centre)*(1/len(GV[g])**0.3))
attached={root:None}; info={}
cand={}
for c in range(len(G)):
    for p in range(len(G)):
        if c!=p: cand[(c,p)]=gjoint(c,p)
while len(attached)<len(G):
    best=None
    for c in range(len(G)):
        if c in attached: continue
        for p in attached:
            pw,err,dp,dc=cand[(c,p)]
            cost=err*20+dp+dc
            if best is None or cost<best[0]: best=(cost,c,p,pw,err,dp,dc)
    cost,c,p,pw,err,dp,dc=best; attached[c]=p; info[c]=(pw,err,dp,dc)
def label(g): return '/'.join(str(i) for i in G[g])
def show(g,depth=0):
    extra='' if g==root else '  pivot %s err %.3f dpar %.2f dchild %.2f'%(np.round(info[g][0],2),*info[g][1:])
    print('  '*depth+'bone %d [parts %s] y%.1f'%(g,label(g),GV[g][:,1].mean())+extra)
    for c,p in attached.items():
        if p==g: show(c,depth+1)
show(root)
pickle.dump(dict(groups=G,parent=attached,info=info,root=root),open('tree.pkl','wb'))
