#include "fluid.h"
#include "fluid_source.h"
#include <model/metrics/_metrics.h>
#include <model/chem/_chem.h>
#include <model/emfield/_emfield.h>
#include <model/thermo/_thermo.h>
#include <model/_model.h>


#ifdef _2D
static void find_Saxi(np_t *np, gl_t *gl, long l, flux_t S){
  long flux;
  double x1, V1;

  for (flux=0; flux<nf; flux++) S[flux]=0.0e0;

  if (gl->model.fluid.AXISYMMETRIC) {
    x1=np[l].bs->x[1];
    V1=_V(np[l],1);
    if (x1<1e-15) fatal_error("No node must lie on or below the y=0 axis when AXISYMMETRIC is set to TRUE.");
    for (flux=0; flux<nf; flux++){
      S[flux]=(-1.0/x1)*V1*np[l].bs->U[flux];
    }
  }
}
#else
static void find_Saxi(np_t *np, gl_t *gl, long l, flux_t S){
  long flux;
  for (flux=0; flux<nf; flux++){
    S[flux]=0.0e0;
  }
}
#endif


static void find_Schem(np_t *np, gl_t *gl, long l, flux_t S){
  spec_t W,rhok;
  long flux,spec;
  double Estar;
  for (flux=0; flux<nf; flux++){
    S[flux]=0.0;
  }
  for (spec=0; spec<ns; spec++) rhok[spec]=_rhok(np[l],spec);
  Estar=0.0;
  find_W(rhok, _T(np[l],gl), _T(np[l],gl), _T(np[l],gl), Estar, _Qbeam(np[l],gl), W);
  for (spec=0; spec<ns; spec++){
    S[spec]=W[spec];
  }
}


void find_Sstar(np_t *np, gl_t *gl, long l, flux_t S){
  flux_t Schem,Saxisymmetric;
  long flux;

  if (gl->model.fluid.REACTING) find_Schem(np,gl,l,Schem);
    else set_vector_to_zero(Schem);
  find_Saxi(np, gl, l, Saxisymmetric);

  for (flux=0; flux<nf; flux++){
    S[flux]=_Omega(np[l],gl)*(Saxisymmetric[flux]+Schem[flux]);
  }
}


#ifdef _2D
static void find_dSaxi_dU(np_t *np, gl_t *gl, long l, sqmat_t dS_dU){
  long row,col;
  for (row=0; row<nf; row++){
    for (col=0; col<nf; col++){
      dS_dU[row][col]=0.0e0;
    }
  }
}
#else
static void find_dSaxi_dU(np_t *np, gl_t *gl, long l, sqmat_t dS_dU){
  long row,col;
  for (row=0; row<nf; row++){
    for (col=0; col<nf; col++){
      dS_dU[row][col]=0.0e0;
    }
  }
}
#endif


static void find_dSchem_dU(np_t *np, gl_t *gl, long l, sqmat_t dS_dU){
  long k,s;
  spec_t mu,rhok,dWdT,dWdTe,dWdTv,dWdQbeam;
  spec2_t dWdrhok;
  flux_t dTdU;
  double Estar;

  Estar=0.0;  
  for (s=0; s<ns; s++) rhok[s]=_rhok(np[l],s);
  for (s=0; s<ns; s++) mu[s]=_mu(np,gl,l,s);

  find_dW_dx(rhok, mu, _T(np[l],gl), _T(np[l],gl), _T(np[l],gl), Estar,_Qbeam(np[l],gl),
           dWdrhok, dWdT, dWdTe, dWdTv, dWdQbeam);
  find_dT_dU(np[l], gl, dTdU);
  for ( k = 0; k < nf; k++ ){
    for ( s = 0; s < nf; s++ ){
      dS_dU[k][s]=0.0e0;
    }
  }
	
  for ( k = 0; k < ns; k++ ){
    for (s=0; s<ns; s++)  dS_dU[k][s]+=dWdrhok[k][s];
    for (s=0; s<nf; s++)  dS_dU[k][s]+=dWdT[k]*dTdU[s];
    for (s=0; s<nf; s++)  dS_dU[k][s]+=dWdTv[k]*dTdU[s];
    for (s=0; s<nf; s++)  dS_dU[k][s]+=dWdTe[k]*dTdU[s];
  }
}


void test_dSchem_dU(np_t *np, gl_t *gl, long l){
  long spec;
  double Estar;
  spec_t rhok,mu;
  Estar=0.0;
  for (spec=0; spec<ns; spec++) rhok[spec]=_rhok(np[l],spec);
  for (spec=0; spec<ns; spec++) mu[spec]=_mu(np,gl,l,spec);
  test_dW_dx(rhok, mu,_T(np[l],gl), _T(np[l],gl), _T(np[l],gl), Estar, _Qbeam(np[l],gl));
}


void find_dSstar_dUstar(np_t *np, gl_t *gl, long l, sqmat_t dSstar_dUstar){
  long col,row;
  sqmat_t dSchemdU,dSaxidU;
  if (gl->model.fluid.REACTING) find_dSchem_dU(np,gl,l,dSchemdU);
    else set_matrix_to_zero(dSchemdU);
  find_dSaxi_dU(np,gl,l,dSaxidU);
  for (row=0; row<nf; row++){
    for (col=0; col<nf; col++){
      dSstar_dUstar[row][col]=dSchemdU[row][col]+dSaxidU[row][col];
    }
  }
}

