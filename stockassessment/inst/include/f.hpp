template <class Type>
Type trans(Type x){
  return Type(2)/(Type(1) + exp(-Type(2) * x)) - Type(1);
}

template<class Type>
Type logdrobust(Type x, Type p){
  Type logres=log((1.0-p)*dnorm(x,Type(0.0),Type(1.0),false)+p*dt(x,Type(1),false));
  return logres;
}
VECTORIZE2_tt(logdrobust)


#define TYPEDEFS(scalartype_)			\
public:						\
typedef scalartype_ scalartype;			\
typedef vector<scalartype> vectortype;		\
typedef matrix<scalartype> matrixtype;		\
typedef array<scalartype> arraytype

#define VARIANCE_NOT_YET_IMPLEMENTED            \
private:                                        \
vectortype variance(){return vectortype();}     \
public:


template <class scalartype_>
class MVMIX_t{
  TYPEDEFS(scalartype_);
  scalartype logdetS; /* log-determinant of Q */
  scalartype p; /*fraction t*/
  matrixtype Sigma;   /* Keep for convenience - not used */
  matrixtype inv_L_Sigma; /* Used by simulate() */
public:
  MVMIX_t(){}
  MVMIX_t(matrixtype Sigma_, scalartype p_){
    setSigma(Sigma_);
    p=p_;
  }
  matrixtype cov(){return Sigma;}
  void setSigma(matrixtype Sigma_){
    Sigma = Sigma_;
    matrixtype I(Sigma.rows(),Sigma.cols());
    I.setIdentity();
    Eigen::LDLT<Eigen::Matrix<scalartype,Eigen::Dynamic,Eigen::Dynamic> > ldlt(Sigma);
    vectortype D = ldlt.vectorD();
    logdetS = D.log().sum();
    Eigen::LLT<Eigen::Matrix<scalartype,Eigen::Dynamic,Eigen::Dynamic> > llt(Sigma);
    matrixtype L_Sigma = llt.matrixL();
    inv_L_Sigma = L_Sigma.inverse();
  }
  /** \brief Evaluate the negative log density */
  scalartype operator()(vectortype x){
    vectortype z=inv_L_Sigma*x;
    return -sum(logdrobust(z,p))+scalartype(0.5)*logdetS;
  }
};

template <class scalartype>
MVMIX_t<scalartype> MVMIX(matrix<scalartype> Sigma){
  return MVMIX_t<scalartype>(Sigma);
}






template <class Type>
Type nllF(confSet &conf, paraSet<Type> &par, array<Type> &logF, data_indicator<vector<Type>,Type> &keep, objective_function<Type> *of){
  using CppAD::abs;
  Type nll=0; 
  int stateDimF=logF.dim[0];
  int timeSteps=logF.dim[1];
  int stateDimN=conf.keyLogFsta.dim[1];
  vector<Type> sdLogFsta=exp(par.logSdLogFsta);
  array<Type> resF(stateDimF,timeSteps-1);
  matrix<Type> fvar(stateDimF,stateDimF);
  matrix<Type> fcor(stateDimF,stateDimF);
  vector<Type> fsd(stateDimF);  

  if(conf.corFlag==0){
    fcor.setZero();
  }

  for(int i=0; i<stateDimF; ++i){
    fcor(i,i)=1.0;
  }

  if(conf.corFlag==1){
    for(int i=0; i<stateDimF; ++i){
      for(int j=0; j<i; ++j){
        fcor(i,j)=trans(par.itrans_rho(0));
        fcor(j,i)=fcor(i,j);
      }
    } 
  }

  if(conf.corFlag==2){
    for(int i=0; i<stateDimF; ++i){
      for(int j=0; j<i; ++j){
        fcor(i,j)=pow(trans(par.itrans_rho(0)),abs(Type(i-j)));
        fcor(j,i)=fcor(i,j);
      }
    } 
  }

  int i,j;
  for(i=0; i<stateDimF; ++i){
    for(j=0; j<stateDimN; ++j){
      if(conf.keyLogFsta(0,j)==i)break;
    }
    fsd(i)=sdLogFsta(conf.keyVarF(0,j));
  }
 
  for(i=0; i<stateDimF; ++i){
    for(j=0; j<stateDimF; ++j){
      fvar(i,j)=fsd(i)*fsd(j)*fcor(i,j);
    }
  }
  //density::MVNORM_t<Type> neg_log_densityF(fvar);
  MVMIX_t<Type> neg_log_densityF(fvar,Type(0.05));
  Eigen::LLT< Matrix<Type, Eigen::Dynamic, Eigen::Dynamic> > lltCovF(fvar);
  matrix<Type> LF = lltCovF.matrixL();
  matrix<Type> LinvF = LF.inverse();

  for(int i=1;i<timeSteps;i++){
    resF.col(i-1) = LinvF*(vector<Type>(logF.col(i)-logF.col(i-1)));    
    nll+=neg_log_densityF(logF.col(i)-logF.col(i-1)); // F-Process likelihood
    SIMULATE_F(of){
      if(conf.simFlag==0){
        //logF.col(i)=logF.col(i-1)+neg_log_densityF.simulate();
      }
    }
  }

  if(CppAD::Variable(keep.sum())){ // add wide prior for first state, but _only_ when computing ooa residuals
    Type huge = 10;
    for (int i = 0; i < stateDimF; i++) nll -= dnorm(logF(i, 0), Type(0), huge, true);  
  } 

  if(conf.resFlag==1){
    ADREPORT_F(resF,of);
  }
  return nll;
}
