import pickle, numpy as np
D=pickle.load(open('rigdata.pkl','rb'))
keys=D['keys']; n=len(keys)
allposes=[D['rest']]+D['poses']
inv=np.linalg.inv
def rel(c,p):
    """relative transforms of part c in part p's frame, over poses where both exist"""
    return [allposes[k][keys[c]]@inv(allposes[k][keys[p]]) for k in range(len(allposes)) if keys[c] in allposes[k] and keys[p] in allposes[k]]
def rigid_err(c,p):
    R=rel(c,p); return max(np.abs(r-R[0]).max() for r in R)
def joint(c,p):
    """least-squares pivot: point x (child local coords) with x@A_k constant (=t) over poses"""
    R=rel(c,p); rows=[];rhs=[]
    for A in R:
        # x@A[:3,:3] + A[3,:3] - t = 0  -> unknowns [x(3), t(3)]
        for j in range(3):
            row=np.zeros(6); row[:3]=A[:3,j]; row[3+j]=-1; rows.append(row); rhs.append(-A[3,j])
    sol,res,rk,sv=np.linalg.lstsq(np.array(rows),np.array(rhs),rcond=None)
    err=np.sqrt(np.mean((np.array(rows)@sol-np.array(rhs))**2))
    return sol[:3],err,len(R)
if __name__=='__main__':
    names=['%d:nv%d/m%d y%.1f'%(i,k[0],k[1],D['rest'][k][3,1]) for i,k in enumerate(keys)]
    E=np.full((n,n),np.inf); RG=np.zeros((n,n))
    for c in range(n):
        for p in range(n):
            if c==p: continue
            RG[c,p]=rigid_err(c,p); E[c,p]=joint(c,p)[1]
    pickle.dump((E,RG,names),open('joints.pkl','wb'))
    for c in range(n):
        order=np.argsort(E[c])[:3]
        print('%-22s'%names[c],' | '.join('%s err %.3f rigid %.3f'%(names[p].split(' ')[0],E[c,p],RG[c,p]) for p in order))
