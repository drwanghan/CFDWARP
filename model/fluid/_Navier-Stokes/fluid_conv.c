#include "fluid.h"
#include "fluid_conv.h"
#include <model/share/fluid_share.h>
#include <model/metrics/_metrics.h>
#include <model/thermo/_thermo.h>
#include <model/emfield/_emfield.h>
#include <model/_model.h>


/* MUSCLVARS1 extrapolate temperature
   MUSCLVARS2 extrapolate Pstar */
#define MUSCLVARS1 1
#define MUSCLVARS2 2
#define MUSCLVARS MUSCLVARS2


/* 
   EIGENSET2 is the new eigenstructure which permits the solution to be both positivity-preserving
             and high-resolution (by preventing any of the characteristic variables to become zero */ 


/* When using eigenset2, rearrange metrics when a singularity occurs within the left eigenvector
   matrix. Only rearrange the metrics part of the left and right eigenvectors and leave the metrics
   of the eigenvalues as they are. */
#define EIGENSET2 2
#define EIGENSET EIGENSET2


double _htstar(np_t np, gl_t *gl){
  double ret;
  assert_np(np,_rho(np)!=0.0e0);
  ret=_etstar(np)+_P(np,gl)/_rho(np);
  return(ret);
}


void find_Fstar(np_t np, gl_t *gl, long theta, flux_t Fstar){
  double Vstar,P,Omega;
  long spec,dim;
  
  Vstar=_Vstar(np,theta);
  P=_P(np,gl);
  Omega=_Omega(np,gl);
  for (spec=0; spec<ns; spec++) Fstar[spec]=np.bs->U[spec]*Vstar*Omega;
  for (dim=0; dim<nd; dim++)
    Fstar[ns+dim]=Omega*np.bs->U[ns+dim]*Vstar+Omega*_X(np,theta,dim)*(P);
  Fstar[fluxet]=Omega*np.bs->U[fluxet]*Vstar+Omega*Vstar*(P);
}


void find_Fstar_given_metrics(np_t np, gl_t *gl, metrics_t metrics, long theta, flux_t Fstar){
  double Vstar,P,Omega;
  long spec,dim;
  Vstar=0.0e0;
  for (dim=0; dim<nd; dim++){
    Vstar+=metrics.X2[theta][dim]*_V(np,dim);
  }
  P=_P(np,gl);
  Omega=metrics.Omega;
  for (spec=0; spec<ns; spec++) Fstar[spec]=np.bs->U[spec]*Vstar*Omega;
  for (dim=0; dim<nd; dim++)
    Fstar[ns+dim]=Omega*np.bs->U[ns+dim]*Vstar+Omega*metrics.X2[theta][dim]*(P);
  Fstar[fluxet]=Omega*np.bs->U[fluxet]*Vstar+Omega*Vstar*(P);
}


void find_dFstar_dUstar(np_t np, gl_t *gl, long theta, sqmat_t A){
  double htstar,Vstar,rho;
  double dPdrhoetstar;
  spec_t dPdrhok;
  dim_t dPdrhoV;
  long row,col,dim,spec;

  Vstar=_Vstar(np,theta);
  htstar=_htstar(np,gl);
  rho=_rho(np);
  find_dP_dx(np, gl, &dPdrhoetstar, dPdrhok, dPdrhoV);
  for (row=0; row<nf; row++){
    for (col=0; col<nf; col++){
      A[row][col]=0.0e0;
    }
  }
  assert_np(np,rho!=0.0e0);
  for (row=0; row<ns; row++){
    for (col=0; col<ns; col++){
      A[row][col]=-Vstar*_rhok(np,row)/rho;
    }
  }
  for (spec=0; spec<ns; spec++){
    A[spec][spec]=A[spec][spec]+Vstar;
    for (dim=0; dim<nd; dim++){
      A[ns+dim][spec]=_X(np,theta,dim)*dPdrhok[spec]-_V(np,dim)*Vstar;
      A[spec][ns+dim]=_X(np,theta,dim)*_rhok(np,spec)/rho;
    }
    A[fluxet][spec]=Vstar*dPdrhok[spec]-htstar*Vstar;
  }
  for (row=0; row<nd; row++){
    for (col=0; col<nd; col++){
      A[ns+row][ns+col]=_X(np,theta,row)*dPdrhoV[col]
                                 +_X(np,theta,col)*_V(np,row);
    }
  }
  for (dim=0; dim<nd; dim++){
    A[ns+dim][ns+dim]=A[ns+dim][ns+dim]+Vstar;
    A[ns+dim][fluxet]=_X(np,theta,dim)*dPdrhoetstar;
    A[fluxet][ns+dim]=Vstar*dPdrhoV[dim]+_X(np,theta,dim)*htstar;
  }
  A[fluxet][fluxet]=Vstar+Vstar*dPdrhoetstar;
}


void find_Fstar_from_jacvars(jacvars_t jacvars, metrics_t metrics, flux_t Fstar){
  long spec,dim;
  double Vstar;

  Vstar=0.0e0;
  for (dim=0; dim<nd; dim++){
    Vstar=Vstar+metrics.X[dim]*jacvars.V[dim];
  }
  for (spec=0; spec<ns; spec++){
    Fstar[spec]=metrics.Omega*jacvars.rho*Vstar*jacvars.w[spec];
  }
  for (dim=0; dim<nd; dim++){
    Fstar[ns+dim]=metrics.Omega*jacvars.rho*jacvars.V[dim]*Vstar+
                   metrics.Omega*metrics.X[dim]*jacvars.P;
  }
  Fstar[fluxet]=metrics.Omega*jacvars.rho*Vstar*jacvars.htstar;
}


void find_A_from_jacvars(jacvars_t jacvars, metrics_t metrics, sqmat_t A){
  double htstar,Vstar;
  double dPdrhoetstar;
  spec_t dPdrhok;
  long dim,row,col,spec;
  Vstar=0.0e0;
  for (dim=0; dim<nd; dim++){
    Vstar=Vstar+metrics.X[dim]*jacvars.V[dim];
  }
  htstar=jacvars.htstar;
  dPdrhoetstar=jacvars.dPdrhoetstar;
  for (spec=0; spec<ns; spec++){
    dPdrhok[spec]=jacvars.dPdrhok[spec];
  }
  for (row=0; row<nf; row++){
    for (col=0; col<nf; col++){
      A[row][col]=0.0e0;
    }
  }
  for (row=0; row<ns; row++){
    for (col=0; col<ns; col++){
      A[row][col]=-Vstar*jacvars.w[row];
    }
  }
  for (spec=0; spec<ns; spec++){
    A[spec][spec]=A[spec][spec]+Vstar;
    for (dim=0; dim<nd; dim++){
      A[ns+dim][spec]=metrics.X[dim]*dPdrhok[spec]-jacvars.V[dim]*Vstar;
      A[spec][ns+dim]=metrics.X[dim]*jacvars.w[spec];
    }
    A[fluxet][spec]=Vstar*dPdrhok[spec]-htstar*Vstar;
  }
  for (row=0; row<nd; row++){
    for (col=0; col<nd; col++){
      A[ns+row][ns+col]=metrics.X[row]*jacvars.dPdrhou[col]
                                 +metrics.X[col]*jacvars.V[row];
    }
  }
  for (dim=0; dim<nd; dim++){
    A[ns+dim][ns+dim]=A[ns+dim][ns+dim]+Vstar;
    A[ns+dim][fluxet]=metrics.X[dim]*dPdrhoetstar;
    A[fluxet][ns+dim]=Vstar*jacvars.dPdrhou[dim]+metrics.X[dim]*htstar;
  }
  A[fluxet][fluxet]=Vstar+Vstar*dPdrhoetstar;
}


double _a_from_jacvars(jacvars_t jacvars){
  double ret;
  long dim,spec;
  double dPdrho,q2;
  dPdrho=0.0e0;
  for (spec=0; spec<ns; spec++){
    dPdrho=dPdrho+jacvars.w[spec]*jacvars.dPdrhok[spec];
  }
  q2=0.0e0;
  for (dim=0; dim<nd; dim++){
    q2=q2+sqr(jacvars.V[dim]);
  }
  if (dPdrho+jacvars.dPdrhoetstar*(jacvars.htstar-q2)<=0.0e0){
    printf("dPdrho=%E\n",dPdrho);
    printf("dPdrhoetstar=%E\n",jacvars.dPdrhoetstar);
    printf("htstar=%E\n",jacvars.htstar);
    printf("q2=%E\n",q2);
    for (spec=0; spec<ns; spec++){
      printf("w[%ld]=%E   dPdrhok[%ld]=%E\n",spec,jacvars.w[spec],spec,jacvars.dPdrhok[spec]);
    }
    fatal_error("Problem with dPrhok in _a_from_jacvars.");
  }
  assert(dPdrho+jacvars.dPdrhoetstar*(jacvars.htstar-q2)>0.0e0);
  ret=sqrt(dPdrho+jacvars.dPdrhoetstar*(jacvars.htstar-q2));
  return(ret);
}


double _eta_from_jacvars(jacvars_t jacvars){
  spec_t nu;
  double eta,kappa;
  find_nuk_eta_kappa(jacvars.w, jacvars.rho, jacvars.T,  nu, &eta, &kappa);
  return(eta);
}


double _V_from_jacvars(jacvars_t jacvars, long dim){
  return(jacvars.V[dim]);
}


void find_Lambda_from_jacvars(jacvars_t jacvars, metrics_t metrics, sqmat_t lambda){
  double sum,Vstar;
  long flux,dim,row,col;
  double a;
  Vstar=0.0e0;
  for (dim=0; dim<nd; dim++){
    Vstar=Vstar+metrics.X[dim]*jacvars.V[dim];
  }
  for (row=0; row<nf; row++){
    for (col=0; col<nf; col++){
      lambda[row][col]=0.0e0;
    }
  }
  for (flux=0; flux<nf; flux++){
      lambda[flux][flux]=Vstar;
  }
  sum=0.0e0;
  for (dim=0; dim<nd; dim++){
    sum=sum+sqr(metrics.X[dim]);
  }
  assert(sum>0.0e0);
  sum=sqrt(sum);
  a=_a_from_jacvars(jacvars);
  lambda[fluxet-1][fluxet-1]=Vstar+a*sum;
  lambda[fluxet][fluxet]=Vstar-a*sum;
}


void find_Linv_from_jacvars_eigenset2(jacvars_t jacvars, metrics_t metrics, sqmat_t R){
  double H,Vstar,a;
  double dPdrhoetstar,Xmag,q2,Vx,Vy,Xhat1,Xhat2;
  spec_t dPdrhok,w;
  long row,col,spec,dim;
  dim_t Xhat;
#ifdef _3D
  double Vz,Xhat3;
#endif

  rearrange_metrics_eigenset2(&metrics);

  Xmag=0.0e0;
  Vstar=0.0e0;
  for (dim=0; dim<nd; dim++){
    Xmag+=sqr(metrics.X[dim]);
    Vstar=Vstar+metrics.X[dim]*jacvars.V[dim];
  }
  assert(Xmag>0.0e0);
  Xmag=sqrt(Xmag);
  assert(Xmag!=0.0e0);
  for (dim=0; dim<nd; dim++){
    Xhat[dim]=metrics.X[dim]/Xmag;
  }
  H=jacvars.htstar;
  a=_a_from_jacvars(jacvars);
  for (spec=0; spec<ns; spec++){
    dPdrhok[spec]=jacvars.dPdrhok[spec];
    w[spec]=jacvars.w[spec];
  }
  dPdrhoetstar=jacvars.dPdrhoetstar;
  for (row=0; row<nf; row++){
    for (col=0; col<nf; col++){
      R[row][col]=0.0e0;
    }
  }
  q2=0.0e0;
  for (dim=0; dim<nd; dim++){
   q2=q2+sqr(jacvars.V[dim]);
  }
  assert(dPdrhoetstar!=0.0e0);

#ifdef _2D
  Vx=jacvars.V[0];
  Vy=jacvars.V[1];
  Xhat1=Xhat[0];
  Xhat2=Xhat[1];
  for (spec=0; spec<ns; spec++){
    R[spec][spec]=1.0;
    R[spec][ns]=w[spec];
    R[spec][ns+1]=w[spec];
    R[spec][ns+2]=w[spec];
    R[ns][spec]=Vx + a*Xhat2;
    R[ns+1][spec]=Vy - a*Xhat1;
    R[fluxet][spec]=q2 - dPdrhok[spec]/dPdrhoetstar + a*(Vx*Xhat2 - Vy*Xhat1);
  }
  
  R[ns][ns]=Vx - a*Xhat2;
  R[ns][ns+1]=Vx + a*Xhat1;
  R[ns][fluxet]=Vx - a*Xhat1;

  R[ns+1][ns]=Vy + a*Xhat1;
  R[ns+1][ns+1]=Vy + a*Xhat2;
  R[ns+1][fluxet]=Vy - a*Xhat2;

  R[fluxet][ns]=H - a*a/dPdrhoetstar + a*(Vy*Xhat1 - Vx*Xhat2);
  R[fluxet][ns+1]=H + a*(Vx*Xhat1 + Vy*Xhat2);
  R[fluxet][fluxet]=H - a*(Vx*Xhat1 + Vy*Xhat2);
#endif

#ifdef _3D
  double a2;
  a2=sqr(a);
  Vx=jacvars.V[0];
  Vy=jacvars.V[1];
  Vz=jacvars.V[2];
  Xhat1=Xhat[0];
  Xhat2=Xhat[1];
  Xhat3=Xhat[2];

  for (spec=0; spec<ns; spec++){
    R[spec][spec]=1.0;
    R[spec][ns]=w[spec];
    R[spec][ns+1]=w[spec];
    R[spec][ns+2]=w[spec];
    R[spec][ns+3]=w[spec];
    R[ns][spec]=Vx + a*(Xhat2 + Xhat3);
    R[ns+1][spec]=Vy - a*Xhat1;
    R[ns+2][spec]=Vz - a*Xhat1;
    R[fluxet][spec]=q2 - dPdrhok[spec]/dPdrhoetstar + a*Vx*(Xhat2 + Xhat3) - a*(Vy + Vz)*Xhat1;
  }

  R[ns][ns]=Vx - a*Xhat2;
  R[ns][ns+1]=Vx - a*Xhat3;
  R[ns][ns+2]=Vx + a*Xhat1;
  R[ns][fluxet]=Vx - a*Xhat1;

  R[ns+1][ns]=Vy + a*(Xhat1 + Xhat3);
  R[ns+1][ns+1]=Vy - a*Xhat3;
  R[ns+1][ns+2]=Vy + a*Xhat2;
  R[ns+1][fluxet]=Vy - a*Xhat2;

  R[ns+2][ns]=Vz - a*Xhat2;
  R[ns+2][ns+1]=Vz + a*(Xhat1 + Xhat2);
  R[ns+2][ns+2]=Vz + a*Xhat3;
  R[ns+2][fluxet]=Vz - a*Xhat3;

  R[fluxet][ns]=H - a2/dPdrhoetstar + a*Vy*(Xhat1 + Xhat3) - a*(Vx + Vz)*Xhat2;
  R[fluxet][ns+1]=H - a2/dPdrhoetstar + a*Vz*(Xhat1 + Xhat2) - a*(Vx + Vy)*Xhat3;
  R[fluxet][ns+2]=H + a*(Vx*Xhat1 + Vy*Xhat2 + Vz*Xhat3);
  R[fluxet][fluxet]=H - a*(Vx*Xhat1 + Vy*Xhat2 + Vz*Xhat3);

#endif
}


void find_Linv_from_jacvars(jacvars_t jacvars, metrics_t metrics, sqmat_t R){
  if (EIGENSET==EIGENSET2) find_Linv_from_jacvars_eigenset2(jacvars, metrics, R);
}



void find_Ustar_from_jacvars(jacvars_t jacvars,  metrics_t metrics, flux_t Ustar){
  long dim,flux,spec;

  for (spec=0; spec<ns; spec++) Ustar[spec]=jacvars.rho*jacvars.w[spec];
  for (dim=0; dim<nd; dim++) Ustar[ns+dim]=jacvars.rho*jacvars.V[dim];
  Ustar[fluxet]=jacvars.rho*(jacvars.htstar-jacvars.P/jacvars.rho );
  for (flux=0; flux<nf; flux++) Ustar[flux]=Ustar[flux]*metrics.Omega;
}


void find_LUstar_from_jacvars(jacvars_t jacvars,  metrics_t metrics, flux_t LUstar){
  flux_t Ustar;
  sqmat_t L;
  double a,a2,dPdrhoetstar,Pstar;
  bool FOUND;
  long spec,flux;

  FOUND=FALSE;
  if (EIGENSET==EIGENSET2){
    rearrange_metrics_eigenset2(&metrics);
#ifdef _2D
    a=_a_from_jacvars(jacvars);
    a2=sqr(a);
    Pstar=jacvars.P;
    dPdrhoetstar=jacvars.dPdrhoetstar;
    for (spec=0; spec<ns; spec++) LUstar[spec]=jacvars.w[spec]*Pstar*dPdrhoetstar/(2.0*a2);
    LUstar[ns]=Pstar*dPdrhoetstar/(2.0*a2);
    LUstar[ns+1]=(jacvars.rho*a2 - Pstar*dPdrhoetstar)/(2.0*a2);
    LUstar[fluxet]=(jacvars.rho*a2 - Pstar*dPdrhoetstar)/(2.0*a2);
#endif


#ifdef _3D
    a=_a_from_jacvars(jacvars);
    a2=sqr(a);
    Pstar=jacvars.P;
    dPdrhoetstar=jacvars.dPdrhoetstar;
    for (spec=0; spec<ns; spec++) LUstar[spec]=jacvars.w[spec]*Pstar*dPdrhoetstar/3.0/a2;
    LUstar[ns]=Pstar*dPdrhoetstar/(3.0*a2);
    LUstar[ns+1]=Pstar*dPdrhoetstar/(3.0*a2);
    LUstar[ns+2]=(jacvars.rho - (Pstar*dPdrhoetstar)/a2)/2.0;
    LUstar[fluxet]=(jacvars.rho - (Pstar*dPdrhoetstar)/a2)/2.0;
#endif 
    for (flux=0; flux<nf; flux++) LUstar[flux]=LUstar[flux]*metrics.Omega;
    FOUND=TRUE;
  }
  if (!FOUND){
    find_L_from_jacvars(jacvars, metrics, L);
    find_Ustar_from_jacvars(jacvars, metrics, Ustar);
    multiply_matrix_and_vector(L,Ustar,LUstar);
  }
}


void find_MUstar_from_jacvars(jacvars_t jacvars,  metrics_t metrics, flux_t MUstar){
  double a,a2,dPdrhoetstar,Pstar;
  long spec,flux;

#ifdef _2D
  a=_a_from_jacvars(jacvars);
  a2=sqr(a);
  Pstar=jacvars.P;
  dPdrhoetstar=jacvars.dPdrhoetstar;
  for (spec=0; spec<ns; spec++) MUstar[spec]=jacvars.w[spec]*Pstar*dPdrhoetstar/(2.0*a2);
  MUstar[ns]=Pstar*dPdrhoetstar/(2.0*a2);
  MUstar[ns+1]=(jacvars.rho*a2 - Pstar*dPdrhoetstar)/(2.0*a2);
  MUstar[fluxet]=(jacvars.rho*a2 - Pstar*dPdrhoetstar)/(2.0*a2);
#endif

#ifdef _3D
  a=_a_from_jacvars(jacvars);
  a2=sqr(a);
  Pstar=jacvars.P;
  dPdrhoetstar=jacvars.dPdrhoetstar;
  for (spec=0; spec<ns; spec++) MUstar[spec]=jacvars.w[spec]*Pstar*dPdrhoetstar/3.0/a2;
  MUstar[ns]=Pstar*dPdrhoetstar/(3.0*a2);
  MUstar[ns+1]=Pstar*dPdrhoetstar/(3.0*a2);
  MUstar[ns+2]=(jacvars.rho - (Pstar*dPdrhoetstar)/a2)/2.0;
  MUstar[fluxet]=(jacvars.rho - (Pstar*dPdrhoetstar)/a2)/2.0;
#endif 

  for (flux=0; flux<nf; flux++) MUstar[flux]=MUstar[flux]*metrics.Omega;
}


void find_L_from_jacvars_eigenset2(jacvars_t jacvars, metrics_t metrics, sqmat_t L){
  double Vstar,a;
  double dPdrhoetstar,Xmag,q2,a2,Vx,Vy,Xhat1,Xhat2;
  spec_t dPdrhok,w;
  long row,col,spec,dim;
  dim_t Xhat;
  double qtmp1;
#ifdef _3D
  double Vz,Xhat3,X1,X2,X3,qtmp2,qtmp3;
  double Xtmp1,Xtmp2,Xtmp3,Xtmp4,Xtmp5,Xtmp6,Xtmp7,Xtmp8,Xtmp9;
#endif

  rearrange_metrics_eigenset2(&metrics);
  Xmag=0.0e0;
  Vstar=0.0e0;
  for (dim=0; dim<nd; dim++){
    Xmag+=sqr(metrics.X[dim]);
    Vstar=Vstar+metrics.X[dim]*jacvars.V[dim];
  }
  assert(Xmag>0.0e0);
  Xmag=sqrt(Xmag);
  assert(Xmag!=0.0e0);
  for (dim=0; dim<nd; dim++){
   Xhat[dim]=metrics.X[dim]/Xmag;
  }
  a=_a_from_jacvars(jacvars);
  a2=sqr(a);
  for (spec=0; spec<ns; spec++){
    dPdrhok[spec]=jacvars.dPdrhok[spec];
    w[spec]=jacvars.w[spec];
  }

  dPdrhoetstar=jacvars.dPdrhoetstar;
  for (row=0; row<nf; row++){
    for (col=0; col<nf; col++){
      L[row][col]=0.0e0;
    }
  }
  q2=0.0e0;
  for (dim=0; dim<nd; dim++){
   q2=q2+sqr(jacvars.V[dim]);
  }
  assert(dPdrhoetstar!=0.0e0);

#ifdef _2D
  Vx=jacvars.V[0];
  Vy=jacvars.V[1];
  Xhat1=Xhat[0];
  Xhat2=Xhat[1];
  qtmp1 = -Vy*Xhat1 + Vx*Xhat2;

  for (spec=0; spec<ns; spec++){
    L[spec][spec]=2.0*a2;
  }

  for (row=0; row<ns; row++){
    for (col=0; col<ns; col++){
      L[row][col]+=-w[row]*(a2+dPdrhok[col]+a*qtmp1);
    }
    L[row][ns]=w[row]*(dPdrhoetstar*Vx+a*Xhat2);
    L[row][ns+1]=w[row]*(dPdrhoetstar*Vy-a*Xhat1);
    L[row][fluxet]=-w[row]*dPdrhoetstar;
  }
  

  for (spec=0; spec<ns; spec++){
    L[ns][spec]=a2 - dPdrhok[spec] + a*qtmp1;
  }
  L[ns][ns]=dPdrhoetstar*Vx - a*Xhat2;
  L[ns][ns+1]=dPdrhoetstar*Vy + a*Xhat1;
  L[ns][fluxet]=-dPdrhoetstar;


  for (spec=0; spec<ns; spec++){
    L[ns+1][spec]=-a*Vstar/Xmag + dPdrhok[spec];
  }
  L[ns+1][ns]=-dPdrhoetstar*Vx + a*Xhat1;
  L[ns+1][ns+1]=-dPdrhoetstar*Vy + a*Xhat2;
  L[ns+1][fluxet]=dPdrhoetstar; 

  for (spec=0; spec<ns; spec++){
    L[fluxet][spec]=a*Vstar/Xmag + dPdrhok[spec];
  }
  L[fluxet][ns]=-(dPdrhoetstar*Vx + a*Xhat1);
  L[fluxet][ns+1]=-(dPdrhoetstar*Vy + a*Xhat2);
  L[fluxet][fluxet]=dPdrhoetstar;

  for (row=0; row<nf; row++){
    for (col=0; col<nf; col++){
      L[row][col]=L[row][col]/(2.0*a2);
    }
  }
#endif




#ifdef _3D
  Vx=jacvars.V[0];
  Vy=jacvars.V[1];
  Vz=jacvars.V[2];
  Xhat1=Xhat[0];
  Xhat2=Xhat[1];
  Xhat3=Xhat[2];
  X1=metrics.X[0];
  X2=metrics.X[1];
  X3=metrics.X[2];

  Xtmp1 = (X1*X1 + X2*(X2 - X3) + 2.0*X1*X3)/(Xmag*(X1 + X2 + X3));
  Xtmp2 = (X1*X1 + 2.0*X1*X2 + X3*(-X2 + X3))/(Xmag*(X1 + X2 + X3));
  Xtmp3 = (X1*(X2 + X3) + 2.0*(X2*X2 + X3*X3))/(Xmag*(X1 + X2 + X3));
  qtmp1 = (-Vz*Xtmp1 - Vy*Xtmp2 + Vx*Xtmp3);
  Xtmp4 = (X1*X1 + X2*X2 - X1*X3 + 2.0*X2*X3)/((X1 + X2 + X3)*Xmag);
  Xtmp5 = (2.0*X1*X2 + X2*X2 - X1*X3 + X3*X3)/((X1 + X2 + X3)*Xmag);
  Xtmp6 = (2.0*X1*X1 + X1*X2 + X3*(X2 + 2.0*X3))/((X1 + X2 + X3)*Xmag);
  qtmp2 = (Vz*Xtmp4 + Vx*Xtmp5 - Vy*Xtmp6);
  Xtmp7 = (-X1*X2 + X2*X2 + 2.0*X1*X3 + X3*X3)/((X1 + X2 + X3)*Xmag);
  Xtmp8 = (2.0*X1*X1 + X1*X3 + X2*(2.0*X2 + X3))/((X1 + X2 + X3)*Xmag);
  Xtmp9 = (X1*X1 - X1*X2 + X3*(2.0*X2 + X3))/((X1 + X2 + X3)*Xmag);
  qtmp3 = (Vx*Xtmp7 - Vz*Xtmp8 + Vy*Xtmp9);

  for (row=0; row<ns; row++){
    L[row][row]=1.0;
    for (col=0; col<ns; col++){
      L[row][col]+=-w[row]*(a*qtmp1 + 2.0*a2 + dPdrhok[col])/(3.0*a2);
    }
    L[row][ns]=w[row]*(dPdrhoetstar*Vx + a*Xtmp3)/(3.0*a2);
    L[row][ns+1]=w[row]*(dPdrhoetstar*Vy - a*Xtmp2)/(3.0*a2);
    L[row][ns+2]=w[row]*(dPdrhoetstar*Vz - a*Xtmp1)/(3.0*a2);
    L[row][fluxet]=-w[row]*dPdrhoetstar/(3.0*a2);
  }

  for (spec=0; spec<ns; spec++){
    L[ns][spec]=(a2 - dPdrhok[spec] + a*qtmp2)/(3.0*a2);
  }
  L[ns][ns]=(dPdrhoetstar*Vx - a*Xtmp5)/(3.0*a2);
  L[ns][ns+1]=(dPdrhoetstar*Vy + a*Xtmp6)/(3.0*a2);
  L[ns][ns+2]=(dPdrhoetstar*Vz - a*Xtmp4)/(3.0*a2);
  L[ns][fluxet]=-dPdrhoetstar/(3.0*a2);

  for (spec=0; spec<ns; spec++){
    L[ns+1][spec]=(a2 - dPdrhok[spec] + a*qtmp3)/(3.0*a2);
  }
  L[ns+1][ns]=(dPdrhoetstar*Vx - a*Xtmp7)/(3.0*a2);
  L[ns+1][ns+1]=(dPdrhoetstar*Vy - a*Xtmp9)/(3.0*a2);
  L[ns+1][ns+2]=(dPdrhoetstar*Vz + a*Xtmp8)/(3.0*a2);
  L[ns+1][fluxet]=-dPdrhoetstar/(3.0*a2);

  for (spec=0; spec<ns; spec++){
    L[ns+2][spec]=(-a*Vstar/Xmag + dPdrhok[spec])/(2.0*a2);
  }
  L[ns+2][ns]=(-dPdrhoetstar*Vx + a*Xhat1)/(2.0*a2);
  L[ns+2][ns+1]=(-dPdrhoetstar*Vy + a*Xhat2)/(2.0*a2);
  L[ns+2][ns+2]=(-dPdrhoetstar*Vz + a*Xhat3)/(2.0*a2);
  L[ns+2][fluxet]=dPdrhoetstar/(2.0*a2);

  for (spec=0; spec<ns; spec++){
    L[fluxet][spec]=(a*Vstar/Xmag + dPdrhok[spec])/(2.0*a2);
  }
  L[fluxet][ns]=(-dPdrhoetstar*Vx - a*Xhat1)/(2.0*a2);
  L[fluxet][ns+1]=(-dPdrhoetstar*Vy - a*Xhat2)/(2.0*a2);
  L[fluxet][ns+2]=(-dPdrhoetstar*Vz - a*Xhat3)/(2.0*a2);
  L[fluxet][fluxet]=dPdrhoetstar/(2.0*a2);

#endif
}


void find_L_from_jacvars(jacvars_t jacvars, metrics_t metrics, sqmat_t L){
  if (EIGENSET==EIGENSET2) find_L_from_jacvars_eigenset2(jacvars, metrics, L);
}


void find_M_from_jacvars(jacvars_t jacvars, metrics_t metrics, sqmat_t M){
  fatal_error("find_M_from_jacvars not yet implemented");  
}

void find_Minv_from_jacvars(jacvars_t jacvars, metrics_t metrics, sqmat_t M){
  fatal_error("find_Minv_from_jacvars not yet implemented");  
}

void find_jacvars_at_interface_Roe_average(jacvars_t jacvarsL, jacvars_t jacvarsR, gl_t *gl, long theta, jacvars_t *jacvars){
  spec_t rhok;
  long dim,spec;
  double etstar,sum,kL,kR,T,rho;
  spec_t dedrhok;


  assert(jacvarsL.rho>=0.0);
  assert(jacvarsR.rho>=0.0);

  kL=1.0e0/(sqrt(jacvarsR.rho/jacvarsL.rho)+1.0e0);
  kR=kL*sqrt(jacvarsR.rho/jacvarsL.rho);
    
  rho=sqrt(jacvarsL.rho*jacvarsR.rho);
  jacvars->rho=rho;
  jacvars->htstar=kR*jacvarsR.htstar+kL*jacvarsL.htstar;
  for (spec=0; spec<ns; spec++){
    jacvars->w[spec]=kR*jacvarsR.w[spec]
           +kL*jacvarsL.w[spec];
    rhok[spec]=jacvars->w[spec]*rho;
  }
  sum=0.0e0;
  for (dim=0; dim<nd; dim++){
    jacvars->V[dim]=kR*jacvarsR.V[dim]+kL*jacvarsL.V[dim];
    sum=sum+sqr(jacvars->V[dim]);
  }
  T=_T_from_w_h(jacvars->w, jacvars->htstar-0.5e0*sum);
  // Find Pressure
  jacvars->P=_P_from_w_rho_T(jacvars->w,rho,T);
  // Find dPdx
  etstar = 0.5e0*sum + _e_from_w_T(jacvars->w,T);
  jacvars->dPdrhoetstar=1.0e0/(rho*_de_dP_at_constant_rho(rhok,T));
  find_de_drhok_at_constant_P(rhok,T,dedrhok);
  for (spec=0; spec<ns; spec++){
    jacvars->dPdrhok[spec]=-(jacvars->dPdrhoetstar)*(etstar-sum
                                 +dedrhok[spec]*rho);
  }

  for (dim=0; dim<nd; dim++){
    jacvars->dPdrhou[dim]=-jacvars->V[dim]*(jacvars->dPdrhoetstar);
  }
  jacvars->T=T;

  set_jacvars_eigenconditioning_constants(gl, jacvars);
}


void find_jacvars_at_interface_arith_average(jacvars_t jacvarsL, jacvars_t jacvarsR, gl_t *gl, long theta, jacvars_t *jacvars){
  spec_t rhok;
  long dim,spec;
  double etstar,sum,kL,kR,T,rho;
  spec_t dedrhok;

  kL=0.5;
  kR=0.5;
    
  rho=kL*jacvarsL.rho+kR*jacvarsR.rho;
  jacvars->rho=rho;
  jacvars->htstar=kR*jacvarsR.htstar+kL*jacvarsL.htstar;
  for (spec=0; spec<ns; spec++){
    jacvars->w[spec]=kR*jacvarsR.w[spec]
           +kL*jacvarsL.w[spec];
    rhok[spec]=jacvars->w[spec]*rho;
  }
  sum=0.0e0;
  for (dim=0; dim<nd; dim++){
    jacvars->V[dim]=kR*jacvarsR.V[dim]+kL*jacvarsL.V[dim];
    sum=sum+sqr(jacvars->V[dim]);
  }
  T=_T_from_w_h(jacvars->w, jacvars->htstar-0.5e0*sum);
  // Find Pressure
  jacvars->P=_P_from_w_rho_T(jacvars->w,rho,T);
  // Find dPdx
  etstar = 0.5e0*sum + _e_from_w_T(jacvars->w,T);
  jacvars->dPdrhoetstar=1.0e0/(rho*_de_dP_at_constant_rho(rhok,T));
  find_de_drhok_at_constant_P(rhok,T,dedrhok);
  for (spec=0; spec<ns; spec++){
      jacvars->dPdrhok[spec]=-(jacvars->dPdrhoetstar)*(etstar-sum
                                 +dedrhok[spec]*rho);
  }

  for (dim=0; dim<nd; dim++){
    jacvars->dPdrhou[dim]=-jacvars->V[dim]*(jacvars->dPdrhoetstar);
  }

  jacvars->T=T;

  set_jacvars_eigenconditioning_constants(gl, jacvars);
}


void find_jacvars(np_t np, gl_t *gl, metrics_t metrics, long theta, jacvars_t *jacvars){
  long dim,spec;
  double sum,rho;

  assert_np(np,is_node_resumed(np));
    
  rho=_rho(np);
  jacvars->rho=rho;
  jacvars->htstar=_htstar(np,gl);
  for (spec=0; spec<ns; spec++){
    jacvars->w[spec]=_rhok(np,spec)/rho;
  }
  sum=0.0e0;
  for (dim=0; dim<nd; dim++){
    jacvars->V[dim]=_V(np,dim);
    sum=sum+sqr(jacvars->V[dim]);
  }
  find_dP_dx(np, gl, &(jacvars->dPdrhoetstar), jacvars->dPdrhok, jacvars->dPdrhou);
  jacvars->P=_P(np,gl);
  jacvars->T=_T(np,gl);

  set_jacvars_eigenconditioning_constants(gl, jacvars);
}


void find_musclvars(np_t np, gl_t *gl, flux_t musclvars){
  long dim,spec;
  for (spec=0; spec<ns-1; spec++) musclvars[spec]=_w(np,spec);
  musclvars[ns-1]=_rho(np);
  for (dim=0; dim<nd; dim++) musclvars[ns+dim]=_V(np,dim);
  switch (MUSCLVARS){
    case MUSCLVARS1: 
      musclvars[fluxet]=_T(np,gl);
    break;
    case MUSCLVARS2: 
      musclvars[fluxet]=_Pstar(np,gl);
    break;
  }
}


bool is_musclvar_a_charged_mass_fraction(long flux){
  bool RET;
  if (flux<ncs) RET=TRUE; else RET=FALSE;
  return(RET);
}


bool is_musclvar_a_mass_fraction(long flux){
  bool RET;
  if (flux<ns-1) RET=TRUE; else RET=FALSE;
  return(RET);
}


void find_Ustar_from_musclvars(flux_t musclvars, metrics_t metrics, gl_t *gl, flux_t Ustar){
  double P,rho,emix,sum,T;
  spec_t rhok,w;
  long spec,dim;
  dim_t V;
  bool REFORMAT;

  REFORMAT=FALSE;
  rho=musclvars[ns-1];
  rhok[ns-1]=rho;
  for (spec=0; spec<ns-1; spec++){
    rhok[spec]=musclvars[spec]*rho;
    rhok[ns-1]-=rhok[spec];
  }
  reformat_rhok(gl,rhok,"_muscl",&REFORMAT);

  rho=0.0e0;
  for (spec=0; spec<ns; spec++){
    Ustar[spec]=rhok[spec]*metrics.Omega;
    rho=rho+rhok[spec];
  }

  for (spec=0; spec<ns; spec++){
    w[spec]=rhok[spec]/rho;
  }
  for (dim=0; dim<nd; dim++) V[dim]=musclvars[ns+dim];
  sum=0.0e0;
  for (dim=0; dim<nd; dim++){
    Ustar[ns+dim]=metrics.Omega*rho*V[dim];
    sum=sum+sqr(V[dim]);
  }

  switch (MUSCLVARS){
    case MUSCLVARS1:
      T=musclvars[fluxet];
      reformat_T(gl,&T,"_muscl",&REFORMAT);
    break;
    case MUSCLVARS2:
      P=musclvars[fluxet];
      reformat_P(gl,&P,"_muscl",&REFORMAT);
      T=_T_from_w_rho_P(w,rho,P);
    break;
  }

  emix=_e_from_w_T(w,T);
  Ustar[fluxet]=metrics.Omega*rho*(0.5e0*sum+emix);
}


void find_jacvars_from_musclvars(flux_t musclvars, metrics_t metrics, gl_t *gl, long theta, jacvars_t *jacvars){
  spec_t rhok,w,dedrhok;
  long dim,spec;
  double T,P,sum,rho,etstar;
  dim_t V;
  bool REFORMAT;

  REFORMAT=FALSE;

  rho=musclvars[ns-1];
  rhok[ns-1]=rho;
  for (spec=0; spec<ns-1; spec++){
    rhok[spec]=musclvars[spec]*rho;
    rhok[ns-1]-=rhok[spec];
  }
  reformat_rhok(gl,rhok,"_muscl",&REFORMAT);

  rho=0.0;
  for (spec=0; spec<ns; spec++) {
    rho+=rhok[spec];
  }
  jacvars->rho=rho;
  for (spec=0; spec<ns; spec++){
    w[spec]=rhok[spec]/jacvars->rho;
    jacvars->w[spec]=w[spec];
  }
  sum=0.0e0;
  for (dim=0; dim<nd; dim++){
    V[dim]=musclvars[ns+dim];
    jacvars->V[dim]=V[dim]; 
    sum=sum+sqr(V[dim]);
  }


  switch (MUSCLVARS){
    case MUSCLVARS1:
      T=musclvars[fluxet];
      reformat_T(gl,&T,"_muscl",&REFORMAT);
      P=_P_from_w_rho_T(w,rho,T);
    break;
    case MUSCLVARS2:
      P=musclvars[fluxet];
      reformat_P(gl,&P,"_muscl",&REFORMAT);
      T=_T_from_w_rho_P(w,rho,P);
    break;
  }


  jacvars->P=P;

  etstar = 0.5e0*sum + _e_from_w_T(w,T);
  jacvars->htstar=etstar+P/rho;
  jacvars->dPdrhoetstar=1.0e0/(rho*_de_dP_at_constant_rho(rhok,T));
  find_de_drhok_at_constant_P(rhok,T,dedrhok);
  for (spec=0; spec<ns; spec++){
      jacvars->dPdrhok[spec]=-(jacvars->dPdrhoetstar)*(etstar-sum
                                 +dedrhok[spec]*rho);
  }

  for (dim=0; dim<nd; dim++){
    jacvars->dPdrhou[dim]=-V[dim]*(jacvars->dPdrhoetstar);
  }

  jacvars->T=T;

  set_jacvars_eigenconditioning_constants(gl, jacvars);
}


void find_jacvars_from_U(flux_t U, metrics_t metrics, gl_t *gl, long theta, jacvars_t *jacvars){
  double rho,P,T,q2;
  spec_t rhok,dedrhok;
  long spec,dim;

  rho=0.0;
  for (spec=0; spec<ns; spec++) rho+=U[spec];
  jacvars->rho=rho;
  assert(rho!=0.0);
  for (spec=0; spec<ns; spec++){
    rhok[spec]=U[spec];
    jacvars->w[spec]=rhok[spec]/rho;
  }
  q2=0.0;
  for (dim=0; dim<nd; dim++){
    jacvars->V[dim]=U[ns+dim]/rho; 
    q2+=sqr(jacvars->V[dim]);
  }

  T=_T_from_w_e(jacvars->w,U[fluxet]/rho-0.5e0*q2);
  P=_P_from_w_rho_T(jacvars->w,rho,T);

  jacvars->P=P;
  jacvars->T=T;

  jacvars->htstar=U[fluxet]/rho+P/rho;
  jacvars->dPdrhoetstar=1.0e0/(rho*_de_dP_at_constant_rho(rhok,T));

  find_de_drhok_at_constant_P(rhok,T,dedrhok);
  for (spec=0; spec<ns; spec++){
      jacvars->dPdrhok[spec]=-(jacvars->dPdrhoetstar)*(U[fluxet]/rho-q2
                                 +dedrhok[spec]*rho);
  }

  for (dim=0; dim<nd; dim++){
    jacvars->dPdrhou[dim]=-jacvars->V[dim]*(jacvars->dPdrhoetstar);
  }

  set_jacvars_eigenconditioning_constants(gl, jacvars);
}



void find_Ustar_given_metrics(np_t np, gl_t *gl, metrics_t metrics, flux_t Ustar){
  long flux;
  for (flux=0; flux<nf; flux++)
    Ustar[flux]=np.bs->U[flux]*metrics.Omega;
}


/* Peclet number */ 
double _Pe_from_jacvars(jacvars_t jacvars, metrics_t metrics){
  double Vstar,eta,kappa,Pe,Xmag2;
  long dim;
  spec_t nu;
  Vstar=0.0;
  Xmag2=0.0;
  for (dim=0; dim<nd; dim++){
    Vstar+=jacvars.V[dim]*metrics.X[dim];
    Xmag2+=sqr(metrics.X[dim]);
  }
  find_nuk_eta_kappa(jacvars.w, jacvars.rho, jacvars.T, nu, &eta, &kappa);
  assert(eta>0.0);
  Pe=jacvars.rho/eta*fabs(Vstar)/Xmag2;
  return(Pe);
}


// the Uprime vector is composed of rhok[1..ns], V[1..nd], e
void find_Uprime(np_t np, gl_t *gl, flux_t Uprime){
  long dim,spec;
  double rho,q2;

  // find rho, k, q2
  rho=0.0;
  for (spec=0; spec<ns; spec++) rho+=np.bs->U[spec];
  q2=0.0e0;
  for (dim=0; dim<nd; dim++){
    q2+=sqr(np.bs->U[ns+dim]/rho);
  }

  // set mass conservation flux component
  for (spec=0; spec<ns; spec++) Uprime[spec]=np.bs->U[spec];

  /* set momentum flux components */
  for (dim=0; dim<nd; dim++) Uprime[ns+dim]=np.bs->U[ns+dim]/rho;

  /* set energy flux component */
  Uprime[fluxet]=(np.bs->U[fluxet]-0.5e0*rho*q2)/rho;

}


void find_primvars_from_jacvars(jacvars_t jacvars, spec_t rhok, dim_t V, double *T, double *e){
  double q2,rho;
  long dim,spec;
  spec_t wk;
  q2=0.0;
  for (dim=0; dim<nd; dim++){
    V[dim]=jacvars.V[dim];
    q2+=sqr(V[dim]);
  }
  *e=jacvars.htstar-jacvars.P/jacvars.rho-0.5*q2;
  rho=0.0;
  for (spec=0; spec<ns; spec++){
    rhok[spec]=jacvars.w[spec]*jacvars.rho;
    rho+=rhok[spec];
  }
  for (spec=0; spec<ns; spec++){
    wk[spec]=rhok[spec]/rho;
  }
  *T=_T_from_w_e(wk, *e);
}


void find_dUstar_dUprime_from_jacvars(jacvars_t jacvars, metrics_t metrics, sqmat_t dUstardUprime){
  long dim,row,col,spec;
  dim_t V;
  spec_t rhok;
  double rho,e,T;

  find_primvars_from_jacvars(jacvars, rhok, V, &T, &e);
  rho=jacvars.rho;

  for (row=0; row<nf; row++){
    for (col=0; col<nf; col++){
      dUstardUprime[row][col]=0.0e0;
    }
  }

  // mass conservation rows
  for (spec=0; spec<ns; spec++){
    dUstardUprime[spec][spec]=metrics.Omega;
  }

  // momentum rows
  for (dim=0; dim<nd; dim++){
    dUstardUprime[ns+dim][ns+dim]=rho*metrics.Omega;
    for (spec=0; spec<ns; spec++) {
      dUstardUprime[ns+dim][spec]=V[dim]*metrics.Omega;
    }
  }


  // the total energy row
  // recall that rho*et=rho*e + 0.5*rho*q^2 + rho*k
  //                   =rho*e + 0.5*rho*u*u + 0.5*rho*v*v + rho*k
  //                   =rho*e + 0.5*rho*u^2 + 0.5*rho*v^2 + rho*k   
  dUstardUprime[fluxet][fluxet]=rho*metrics.Omega;
  for (spec=0; spec<ns; spec++) {
    dUstardUprime[fluxet][spec]=e*metrics.Omega;
  }
  for (dim=0; dim<nd; dim++) {
    dUstardUprime[fluxet][ns+dim]=rho*V[dim]*metrics.Omega;
    for (spec=0; spec<ns; spec++) {
      dUstardUprime[fluxet][spec]+=0.5*sqr(V[dim])*metrics.Omega;
    }
  }
}


/* phiL and phiR must be determinative properties (positive)  */
static double _f_Parent_average(double phiL, double phiR, double Mtheta){
  double phi;
  phiL=max(1.0e-99, phiL);
  phiR=max(1.0e-99, phiR);
  phi=min(phiL,phiR);
  return(phi);
}


void find_jacvars_at_interface_Parent_average(jacvars_t jacvarsL, jacvars_t jacvarsR, gl_t *gl, long theta, metrics_t metrics, jacvars_t *jacvars){
  spec_t rhok,rhokL,rhokR;
  long dim,spec;
  double etstar,T,rho,P,q2;
  spec_t dedrhok;
  dim_t VL,VR,V;
  double eL,eR,e,TL,TR;
  double Xmag2,Vstar,Mtheta;

  find_primvars_from_jacvars(jacvarsL, rhokL, VL, &TL, &eL);
  find_primvars_from_jacvars(jacvarsR, rhokR, VR, &TR, &eR);

  q2=0.0e0;
  Vstar=0.0;
  Xmag2=0.0;
  for (dim=0; dim<nd; dim++){
//    V[dim]=0.5*VL[dim]+0.5*VR[dim];
    V[dim]=minmod(VL[dim],VR[dim]);
    jacvars->V[dim]=V[dim];
    q2+=sqr(V[dim]);
    Vstar+=V[dim]*metrics.X[dim];
    Xmag2+=sqr(metrics.X[dim]);
  }
  Mtheta=2.0e0*(fabs(Vstar)/sqrt(Xmag2))/(_a_from_jacvars(jacvarsL)+_a_from_jacvars(jacvarsR));
  rho=0.0;
  for (spec=0; spec<ns; spec++){
    rhok[spec]=_f_Parent_average(rhokL[spec],rhokR[spec],Mtheta);
    rho+=rhok[spec];
  }  
  jacvars->rho=rho;
  for (spec=0; spec<ns; spec++){
    jacvars->w[spec]=rhok[spec]/rho;
  }

  T=_f_Parent_average(TL,TR,Mtheta);
  jacvars->T=T;
  e=_e_from_w_T(jacvars->w,T);
  P=_P_from_w_rho_T(jacvars->w,rho,T);
  jacvars->P=P;
  etstar = 0.5e0*q2 + e;
  jacvars->htstar=etstar+P/rho;
  jacvars->dPdrhoetstar=1.0e0/(rho*_de_dP_at_constant_rho(rhok,T));
  find_de_drhok_at_constant_P(rhok,T,dedrhok);
  for (spec=0; spec<ns; spec++){
      jacvars->dPdrhok[spec]=-(jacvars->dPdrhoetstar)*(etstar-q2
                                 +dedrhok[spec]*rho);
  }
  for (dim=0; dim<nd; dim++){
    jacvars->dPdrhou[dim]=-jacvars->V[dim]*(jacvars->dPdrhoetstar);
  }

  set_jacvars_eigenconditioning_constants(gl, jacvars);
}

