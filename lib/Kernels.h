#include "AdalMatrix.h"
//choose activation neuron function
#define nonlin 0 // 0 --> tansigmoid 		1--> exp_sigmoid

#define t  0.9
//momentum BPA
#define momentum 0.7

//resilient BPA
#define h_big      1.1
#define h_little   0.50

#define dmin 0.005
#define dmax 0.4
//device prototypes
__device__ float Sigmoid( float x );
__device__ float Sig_der( float sigmoid_output );
__device__ float TanSig( float x );
__device__ float TanSig_der( float HyperTan_output );
__device__ float Minimum( float a , float b  );
__device__ float Maximum( float a , float b  );
__device__ float Sign( float x );
__device__ float  Calc_DeltaW_prev( float  grad , float Dij );
__device__ float Diff( float x , float y );
__device__ float Mul( float x , float y );
/*kernel description
 * this implements the Feedforward Kernel
 * A is the vector of the output from the previous level
 * W is the weights of the current level
 * Out is the output vector to the next level of the NN
 * The width of the W matrix must be equal to to A vector size in order to multiply
 * Out will be a vector with the dimension of the w.height which is the rows which corresponds to the neurons of the current level
 * Each thread is neuron of the current level
 * */
//TODO REDUCE!!!!!!
__global__ void Kernel( float *A ,  Matrix W , float *Out ) // vector_size = W.width
{
	int tx =  threadIdx.x;
	float register sum = 0 ;
	int register k ;
	float register a,b ;
//each thread multiplies one row of W with the input Vector A
#pragma unroll
	for(k=0 ; k<W.width  ; k++)
	{
		a = W.elements[ tx*W.width + k] ;
		b = ( k==(W.width-1) )? 1 : A[k]  ;
		sum  += ( a * b ) ;
	}
	Out[tx]= (nonlin)? Sigmoid(sum) : TanSig(sum) ;

}
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//-------------- classic Back Propagation Algorithm -----------------------------------
/*
/*kernel2 description
 * A vector is the output of the previous level ,we need in order to change the weights of the last level
 * this kernel implements the back propagation for the output layer of the neural network
 * Desired is the vector with the target values , the ideal output
 * W is the weights of the output level
 * Out_ff is the output of the feedforward process , vector
 * Out will be a vector with the dimension of the w.height which is the rows which corresponds to the neurons of the current level
 * Each thread is neuron of the current level
 * Delta_val is the delta values of each neuron , it resembles the error tha we back-probagate to the hidden layers , we do that in kernel 3!!!!!
 * */


__global__ void Kernel2( float *A ,  Matrix W , float *Out_ff , float *Desired , float *Delta_val , float lrate )
{
    int tx =  threadIdx.x;
    float register DeltaW = 0 ;
    int register k ;

    float error = Out_ff[tx]-Desired[tx];
    float sigmoid_derivative= (nonlin)?  Sig_der( Out_ff[tx] ) :TanSig_der( Out_ff[tx] )  ;
    float register delta_neuron = error*sigmoid_derivative ;

    Delta_val[tx] = delta_neuron ;

    float register w_old , w_new, previous_level_output ;
#pragma unroll
    for(k=0 ; k<W.width  ; k++)
    {
        w_old = W.elements[ tx*W.width + k] ;
        previous_level_output = ( ( k==(W.width-1) ) ? 1 :  A[k] )  ;
        DeltaW = delta_neuron * previous_level_output * lrate ;
        w_new = w_old - DeltaW ;
        W.elements[ tx*W.width + k] = w_new;
    }
}
/*kernel 3 description
 * A vector is the output of the previous level ,we need in order to change the weights Delta_W of the current level
 * this kernel implements the back propagation for the output layer of the neural network
 * W is the weights of the current layer
 * W_next is the weights of the next layer , we need it in order to compute the delta values of the current level
 * Out_current is the output of the current layer , vector
 * Out_next is the output of the next  layer , we need it in order to compute the delta values of the current level
 * Each thread is neuron of the current level
 * W_next is the updated values !!! ???????????
 *
 * */
__global__ void Kernel3( float *A , Matrix W , float *Out_current , Matrix W_next , float *Out_next , float *Delta_current , float *Delta_next , float lrate  )
{
    int tx =  threadIdx.x;
    float register DeltaW = 0 ;
    int register k ;
    float out_derivative,temp ;

// here we implement the temp = Σ (  δ_next_level_k * W_next( threadIdx.x , k ) , where k is in the range 0-W_next.height
#pragma unroll
    for(k=0 ; k < W_next.height  ; k++)
    {
         temp = Delta_next[k] * W_next.elements[ k*W_next.width +  tx ] ;

    }
    out_derivative =   (nonlin)?  Sig_der( Out_current[tx] ) :TanSig_der( Out_current[tx] )  ;

    Delta_current[tx] = out_derivative * temp ;//??????????????????????????????????????????????????????????????

// in this point we know all the delta values of the current level and we back-propagate to the previous level changing the Weights of the current level
// this procces is the same with the  kernel 2 loop
    float delta_neuron =  Delta_current[tx] ;
    float w_old , w_new, previous_level_output ;
#pragma unroll
    for(k=0 ; k<W.width  ; k++)
        {
            w_old = W.elements[ tx*W.width + k] ;
            previous_level_output =  (( k==(W.width-1) ) ? 1 :  A[k] )  ;
            DeltaW = delta_neuron * previous_level_output*lrate ;
            w_new = w_old - DeltaW;
            W.elements[ tx*W.width + k] = w_new;
        }
}


//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//---------------------Resilient Backprobagation------------------------------
// kernel 4 and 5



//description TODO
__global__ void Kernel4( float *A ,  Matrix W , float *Out_ff , float *Desired , float *Delta_val , float lrate , Matrix Grad_prev , Matrix Dij , Matrix DW_prev , int counter )
{

	int tx =  threadIdx.x;
	float register DeltaW = 0 ;
	int register k ;
	float register out_derivative ;
	float error = Out_ff[tx]-Desired[tx];

	out_derivative= (nonlin)?  Sig_der( Out_ff[tx] ) :TanSig_der( Out_ff[tx] )  ;

	float register delta_neuron = error*out_derivative ;
	Delta_val[tx] = delta_neuron ;
	float register w_old , w_new, previous_level_output,grad_prev,gradient  , Dij_prev , Dij_new, DW_previous , temp ;
#pragma unroll
	for(k=0 ; k<W.width  ; k++)
	{
		DW_previous = DW_prev.elements[ tx*W.width + k];
		grad_prev  = Grad_prev.elements[ tx*W.width + k] ;
		Dij_prev = Dij.elements[ tx*W.width + k] ;
		w_old = W.elements[ tx*W.width + k] ;
		previous_level_output = ( ( k==(W.width-1) ) ? 1 :  A[k] )  ;
		gradient =delta_neuron * previous_level_output ;
		temp = Mul(gradient , grad_prev ) ;
			if(temp>0)
			{
				Dij_new =Minimum( Dij_prev*h_big , dmax )   ;
				DeltaW = -1*Sign(gradient)*Dij_new ;
				Grad_prev.elements[ tx*W.width + k] =  gradient ;
			}
			else if(temp<0 )
			{
				Dij_new =Maximum( (Dij_prev*h_little) , dmin) ;
				DeltaW  = -1*DW_previous ;
				Grad_prev.elements[ tx*W.width + k] = 0 ;
			}
			else
			{
				Dij_new =Dij_prev  ;
				DeltaW = -1*Sign(gradient)*Dij_new ;
				Grad_prev.elements[ tx*W.width + k] =  gradient ;
			}
		w_new = w_old + DeltaW ;
		W.elements[ tx*W.width + k] = w_new;
		Dij.elements[ tx*W.width + k]  = Dij_new ;
		DW_prev.elements[ tx*W.width + k] = DeltaW ;
	}
}

//-----------------------------------------------------------------------


__global__ void Kernel5( float *A , Matrix W , float *Out_current , Matrix W_next , float *Out_next , float *Delta_current , float *Delta_next , float lrate ,  Matrix Grad_prev , Matrix Dij , Matrix DW_prev  , int counter )
{
	int tx =  threadIdx.x;
	float register DeltaW = 0 ;
	int register k ;
	float register OutCurrentDerivative,temp ;

// here we implement the temp = Σ (  δ_next_level_k * W_next( threadIdx.x , k ) , where k is in the range 0-W_next.height
#pragma unroll
	for(k=0 ; k < W_next.height  ; k++)
	{
		 temp = Delta_next[k] * W_next.elements[ k*W_next.width +  tx ] ;

	}

	OutCurrentDerivative = (nonlin)?  Sig_der( Out_current[tx] ) :TanSig_der( Out_current[tx] )  ;

	Delta_current[tx] = OutCurrentDerivative * temp ;

	float delta_neuron =  Delta_current[tx] ;

	float register w_old , w_new, previous_level_output,grad_prev,gradient  , Dij_prev , Dij_new , DW_previous ;
#pragma unroll
	for(k=0 ; k<W.width  ; k++)
	{
		DW_previous = DW_prev.elements[ tx*W.width + k];
		grad_prev  = Grad_prev.elements[ tx*W.width + k] ;
		Dij_prev = Dij.elements[ tx*W.width + k] ;
		w_old = W.elements[ tx*W.width + k] ;
		previous_level_output = ( ( k==(W.width-1) ) ? 1 :  A[k] )  ;
		gradient =delta_neuron * previous_level_output ;
		temp = Mul(gradient , grad_prev ) ;
			if(temp>0)
			{
				Dij_new =Minimum( Dij_prev*h_big , dmax )   ;
				DeltaW = -1*Sign(gradient)*Dij_new ;
				Grad_prev.elements[ tx*W.width + k] =  gradient ;
			}
			else if(temp<0 )
			{
				Dij_new =Maximum( Dij_prev*h_little , dmin) ;
				DeltaW  = -1*DW_previous ;
				Grad_prev.elements[ tx*W.width + k] = 0 ;
			}
			else
			{
				Dij_new =Dij_prev  ;
				DeltaW = -1*Sign(gradient)*Dij_new ;
				Grad_prev.elements[ tx*W.width + k] =  gradient ;
			}
		w_new = w_old + DeltaW ;
		W.elements[ tx*W.width + k] = w_new;
		Dij.elements[ tx*W.width + k]  = Dij_new ;
		DW_prev.elements[ tx*W.width + k] = DeltaW ;
	}
}



//---------------------------------------------------------------
//---------------------------------------------------------------
//---------------------------------------------------------------
//---------------------------------------------------------------
//---------------------------------------------------------------
//---------------------------------------------------------------

//---------------------------------------------------------------
//momentum bpa



__global__ void Kernel_MOM2( float *A ,  Matrix W , float *Out_ff , float *Desired , float *Delta_val , float lrate ,int count , Matrix dW  )
{
    int tx =  threadIdx.x;
    float register DeltaW = 0 ;
    int register k ;

    float error = Out_ff[tx]-Desired[tx];
    float sigmoid_derivative= (nonlin)?  Sig_der( Out_ff[tx] ) :TanSig_der( Out_ff[tx] )  ;
    float register delta_neuron = error*sigmoid_derivative ;

    Delta_val[tx] = delta_neuron ;

    float register w_old , w_new, previous_level_output , dwPrev ;
#pragma unroll
    for(k=0 ; k<W.width  ; k++)
    {
    	dwPrev = dW.elements[ tx*W.width + k];
        w_old = W.elements[ tx*W.width + k] ;
        previous_level_output = ( ( k==(W.width-1) ) ? 1 :  A[k] )  ;
        DeltaW = -1*delta_neuron * previous_level_output * lrate ;
        w_new = w_old + momentum*DeltaW + (1-momentum)*dwPrev ;
        W.elements[ tx*W.width + k] = w_new;
        dW.elements[ tx*W.width + k]= DeltaW ;
    }
}

__global__ void Kernel_MOM3( float *A , Matrix W , float *Out_current , Matrix W_next , float *Out_next , float *Delta_current , float *Delta_next , float lrate ,int count, Matrix dW   )
{
    int tx =  threadIdx.x;
    float register DeltaW = 0 ;
    int register k ;
    float out_derivative,temp ;

// here we implement the temp = Σ (  δ_next_level_k * W_next( threadIdx.x , k ) , where k is in the range 0-W_next.height
#pragma unroll
    for(k=0 ; k < W_next.height  ; k++)
    {
         temp = Delta_next[k] * W_next.elements[ k*W_next.width +  tx ] ;

    }
    out_derivative =   (nonlin)?  Sig_der( Out_current[tx] ) :TanSig_der( Out_current[tx] )  ;
    Delta_current[tx] = out_derivative * temp ;
    float delta_neuron =  Delta_current[tx] ;
    float w_old , w_new, previous_level_output , dwPrev ;
#pragma unroll
    for(k=0 ; k<W.width  ; k++)
        {
    		//dwPrev =(count<=5)? 0 : dW.elements[ tx*W.width + k];
    		dwPrev = dW.elements[ tx*W.width + k];
            w_old = W.elements[ tx*W.width + k] ;
            previous_level_output =  (( k==(W.width-1) ) ? 1 :  A[k] )  ;
            DeltaW = -1*delta_neuron * previous_level_output*lrate ;
            w_new = w_old + momentum*DeltaW + (1-momentum)*dwPrev ;
            W.elements[ tx*W.width + k] = w_new;
            dW.elements[ tx*W.width + k]= DeltaW ;
        }
}








//---------------------------------------------------------------
//---------------------------------------------------------------
//---------------------------------------------------------------
//---------------------------------------------------------------
//---------------------------------------------------------------
//---------------------------------------------------------------

//---------------------------------------------------------------
//QUICK PROPAGATION



__global__ void Kernel_QUICK2( float *A ,  Matrix W , float *Out_ff , float *Desired , float *Delta_val , Matrix DW_prev , Matrix Gradient)
{
    int tx =  threadIdx.x;
    float register DeltaW = 0 ;
    int register k ;

    float error = Out_ff[tx]-Desired[tx];
    float sigmoid_derivative= (nonlin)?  Sig_der( Out_ff[tx] ) :TanSig_der( Out_ff[tx] )  ;
    float register delta_neuron = error*sigmoid_derivative ;

    Delta_val[tx] = delta_neuron ;

    float register w_old , w_new, previous_level_output , grad_pre , grad_cur , dw_prev , lrate , dif;
#pragma unroll
    for(k=0 ; k<W.width  ; k++)
    {
    	grad_pre = Gradient.elements[ tx*W.width + k ] ;
    	dw_prev = DW_prev.elements[ tx*W.width + k ] ;
        w_old = W.elements[ tx*W.width + k] ;
        previous_level_output = ( ( k==(W.width-1) ) ? 1 :  A[k] )  ;
        grad_cur =  delta_neuron * previous_level_output;
        dif = Diff(grad_pre , grad_cur);
        lrate = dw_prev/dif ;
        DeltaW = -1*grad_cur* lrate ;
        w_new = w_old + DeltaW ;
        DW_prev.elements[ tx*W.width + k ] = DeltaW ;
        W.elements[ tx*W.width + k] = w_new;
        Gradient.elements[ tx*W.width + k ] = grad_cur ;
    }
}

__global__ void Kernel_QUICK3( float *A , Matrix W , float *Out_current , Matrix W_next , float *Out_next , float *Delta_current , float *Delta_next , Matrix DW_prev , Matrix Gradient  )
{
    int tx =  threadIdx.x;
    float register DeltaW = 0 ;
    int register k ;
    float out_derivative,temp ;

// here we implement the temp = Σ (  δ_next_level_k * W_next( threadIdx.x , k ) , where k is in the range 0-W_next.height
#pragma unroll
    for(k=0 ; k < W_next.height  ; k++)
    {
         temp = Delta_next[k] * W_next.elements[ k*W_next.width +  tx ] ;

    }
    out_derivative =   (nonlin)?  Sig_der( Out_current[tx] ) :TanSig_der( Out_current[tx] )  ;

    Delta_current[tx] = out_derivative * temp ;

    float delta_neuron =  Delta_current[tx] ;
    float register w_old , w_new, previous_level_output , grad_pre , grad_cur , dw_prev, lrate ,dif ;
 #pragma unroll
     for(k=0 ; k<W.width  ; k++)
     {
		grad_pre = Gradient.elements[ tx*W.width + k ] ;
		dw_prev = DW_prev.elements[ tx*W.width + k ] ;
		w_old = W.elements[ tx*W.width + k] ;
		 previous_level_output = ( ( k==(W.width-1) ) ? 1 :  A[k] )  ;
		 grad_cur =  delta_neuron * previous_level_output;
		 dif = Diff(grad_pre , grad_cur);
		 lrate = dw_prev/dif ;
		 DeltaW = -1*grad_cur* lrate ;
		 w_new = w_old + DeltaW ;
		 DW_prev.elements[ tx*W.width + k ] = DeltaW ;
		 W.elements[ tx*W.width + k] = w_new;
		 Gradient.elements[ tx*W.width + k ] = grad_cur ;
     }
}







//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//------------------------Device Helper Function Calls------------------------

__device__ float  Calc_DeltaW_prev( float  grad , float Dij )
{
    float dw = -1*Sign(grad)*Dij;
	return (-1*dw);
}

__device__ float Sigmoid( float x )
{
	return ( (1)/(1+expf(-x*t))  ) ;
}

__device__ float Sigmoid_Derivative( float x )
{
	float s = (1)/(1+expf(-x*t)) ;
	return ( (s*(1-s))  ) ;
}

__device__ float Sig_der( float sigmoid_output )
{
	float s = sigmoid_output ;
	return ( (s*(1-s))  ) ;
}

////////////////////////////////////////////////////

__device__ float TanSig( float x )
{
	//return ( (1-expf((-2)*x))/(1+expf(2*x))  ) ;
	return ((2)/(1+expf(-2*x))) -1 ;
}

__device__ float TanSig_der( float Tan_output )
{
	float s = Tan_output ;
	 return (1-s*s) ;
}
__device__ float Minimum( float a , float b  )
{
	return ((a<=b) ? a  : b);
}
__device__ float Maximum( float a , float b  )
{
	 return ((a>=b) ? a  : b);
}
__device__ float Sign( float x )
{
	return ((x>0)?1:0 ) ;
}
__device__ float Diff( float x , float y )
{
	float th= 0.001 ;
	if (x>=y)
	{
		if ( (x-y)<th ) return th ;
		return (x-y); //positive
	}
	if ( (y-x)<th  ) return -1*th;
	return ( x-y );
}
__device__ float Mul( float x , float y )
{
	float th= 0.001;
	if (x*y>th)	return +1;
	else if (x*y<(-1*th)) return -1;
	else return 0 ;
}






/*
__global__ void Kernel7( float *A ,  Matrix W , float *Out_ff , float *Desired , float *Delta_val , float lrate ,  Matrix Grad_prev , Matrix dW_prev )
{
    int tx =  threadIdx.x;
    float register DeltaW = 0 ;
    int register k ;

    float error = Out_ff[tx]-Desired[tx];
    float register sigmoid_derivative=  Sig_der( Out_ff[tx] )  ;
    float register delta_neuron = error*sigmoid_derivative ;//eeeeeeeeeeeeeeee we must save this shit??????

    Delta_val[tx] = delta_neuron ;

    float register w_old , w_new, previous_level_output , gradient ;
#pragma unroll
    for(k=0 ; k<W.width  ; k++)
    {
        w_old = W.elements[ tx*W.width + k] ;
        previous_level_output = ( ( k==(W.width-1) ) ? 1 :  A[k] )  ;
        //previous_level_output =   A[k] ;
        gradient =delta_neuron * previous_level_output ;
        DeltaW = -1*gradient * lrate ;
        w_new = w_old + DeltaW ;
        W.elements[ tx*W.width + k] = w_new;
        dW_prev.elements[ tx*W.width + k] = DeltaW ;
        Grad_prev.elements[ tx*W.width + k] =  gradient ;
    }
}
/*kernel 3 description
 * A vector is the output of the previous level ,we need in order to change the weights Delta_W of the current level
 * this kernel implements the back propagation for the output layer of the neural network
 * W is the weights of the current layer
 * W_next is the weights of the next layer , we need it in order to compute the delta values of the current level
 * Out_current is the output of the current layer , vector
 * Out_next is the output of the next  layer , we need it in order to compute the delta values of the current level
 * Each thread is neuron of the current level
 * W_next is the updated values !!! ???????????
 *
 * */


/*

__global__ void Kernel8( float *A , Matrix W , float *Out_current , Matrix W_next , float *Out_next , float *Delta_current , float *Delta_next , float lrate ,  Matrix Grad_prev , Matrix dW_prev  )
{
    int tx =  threadIdx.x;
    float register DeltaW = 0 ;
    int register k ;
    float sigmoid_current_derivative,temp ;

// here we implement the temp = Σ (  δ_next_level_k * W_next( threadIdx.x , k ) , where k is in the range 0-W_next.height
#pragma unroll
    for(k=0 ; k < W_next.height  ; k++)
    {
         temp = Delta_next[k] * W_next.elements[ k*W_next.width +  tx ] ;

    }
    sigmoid_current_derivative =   Sig_der( Out_current[tx]  ) ;

    Delta_current[tx] = sigmoid_current_derivative * temp ;

// in this point we know all the delta values of the current level and we back-propagate to the previous level changing the Weights of the current level
// this procces is the same with the  kernel 2 loop
    float delta_neuron =  Delta_current[tx] ;
    float register w_old , w_new, previous_level_output , gradient ;
#pragma unroll
    for(k=0 ; k<W.width  ; k++)
    {
        w_old = W.elements[ tx*W.width + k] ;
        previous_level_output = ( ( k==(W.width-1) ) ? 1 :  A[k] )  ;
        //previous_level_output =   A[k] ;
        gradient =delta_neuron * previous_level_output ;
        DeltaW = -1*gradient * lrate ;
        w_new = w_old + DeltaW ;
        W.elements[ tx*W.width + k] = w_new;
        dW_prev.elements[ tx*W.width + k] = DeltaW ;
        Grad_prev.elements[ tx*W.width + k] =  gradient ;
    }
}

//-----------------------------------------------------------------------------------
//////////////////////////////////////////////////////////////////////////


__global__ void Kernel99( float *A , Matrix W , float *Out_current , Matrix W_next , float *Out_next , float *Delta_current , float *Delta_next , float lrate  )
{
    int tx =  threadIdx.x;
    float register DeltaW = 0 ;
    int register k ;
    float nonlinder,temp ;

// here we implement the temp = Σ (  δ_next_level_k * W_next( threadIdx.x , k ) , where k is in the range 0-W_next.height
    for(k=0 ; k < W_next.height  ; k++)
    {
         temp = Delta_next[k] * W_next.elements[ k*W_next.width +  tx ] ;

    }
    nonlinder =    (nonlin)?  Sig_der( Out_current[tx] ) :TanSig_der(Out_current[tx] )  ;
    Delta_current[tx] = nonlinder * temp*100000 ;

// in this point we know all the delta values of the current level and we back-propagate to the previous level changing the Weights of the current level
// this procces is the same with the  kernel 2 loop
    float delta_neuron =  (Delta_current[tx])/100000 ;
    float w_old , w_new, previous_level_output ;
#pragma unroll
    for(k=0 ; k<W.width  ; k++)
        {
            w_old = W.elements[ tx*W.width + k] ;
            previous_level_output =  (( k==(W.width-1) ) ? 1 :  A[k] )  ;
            //previous_level_output = A[k]   ;
            DeltaW = delta_neuron * previous_level_output*lrate ;
            w_new = w_old - DeltaW;
            W.elements[ tx*W.width + k] = w_new;
        }
}
*/
