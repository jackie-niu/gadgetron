
#include "hoSPIRIT2DTOperator.h"
#include "hoNDFFT.h"
#include "mri_core_spirit.h"

namespace Gadgetron 
{

template <typename T> 
hoSPIRIT2DTOperator<T>::hoSPIRIT2DTOperator(std::vector<size_t> *dims) : BaseClass(dims)
{
}

template <typename T> 
hoSPIRIT2DTOperator<T>::~hoSPIRIT2DTOperator()
{
}

template <typename T> 
void hoSPIRIT2DTOperator<T>::set_forward_kernel(hoNDArray<T>& forward_kernel, bool compute_adjoint_forward_kernel)
{
    try
    {
        forward_kernel_ = forward_kernel;

        size_t RO     = forward_kernel_.get_size(0);
        size_t E1     = forward_kernel_.get_size(1);
        size_t srcCHA = forward_kernel_.get_size(2);
        size_t dstCHA = forward_kernel_.get_size(3);
        size_t N      = forward_kernel_.get_size(4);

        this->adjoint_kernel_.create(RO, E1, dstCHA, srcCHA, N);

        if (compute_adjoint_forward_kernel)
        {
            adjoint_forward_kernel_.create(RO, E1, srcCHA, srcCHA, N);
        }

        size_t n;
        for (n = 0; n<N; n++)
        {
            hoNDArray<T> kerCurr(RO, E1, srcCHA, dstCHA, this->forward_kernel_.begin() + n*RO*E1*srcCHA*dstCHA);
            hoNDArray<T> adjKerCurr(RO, E1, dstCHA, srcCHA, this->adjoint_kernel_.begin() + n*RO*E1*dstCHA*srcCHA);

            Gadgetron::spirit_image_domain_adjoint_kernel(kerCurr, adjKerCurr);

            if (compute_adjoint_forward_kernel)
            {
                hoNDArray<T> adjFowardKerCurr(RO, E1, srcCHA, srcCHA, this->adjoint_forward_kernel_.begin() + n*RO*E1*srcCHA*srcCHA);
                Gadgetron::spirit_adjoint_forward_kernel(adjKerCurr, kerCurr, adjFowardKerCurr);
            }
        }

        // allocate the helper memory
        res_after_apply_kernel_.create(RO, E1, srcCHA, dstCHA);
        res_after_apply_kernel_dst_.create(RO, E1, dstCHA, srcCHA);

        if(kspace_.get_size(4)>N)
        {
            res_after_apply_kernel_sum_over_.create(RO, E1, dstCHA, kspace_.get_size(4));
            res_after_apply_kernel_sum_over_dst_.create(RO, E1, srcCHA, kspace_.get_size(4));
            kspace_dst_.create(RO, E1, dstCHA, kspace_.get_size(4));
            complexIm_dst_.create(RO, E1, dstCHA, kspace_.get_size(4));
        }
        else
        {
            res_after_apply_kernel_sum_over_.create(RO, E1, dstCHA, N);
            res_after_apply_kernel_sum_over_dst_.create(RO, E1, srcCHA, N);
            kspace_dst_.create(RO, E1, dstCHA, N);
            complexIm_dst_.create(RO, E1, dstCHA, N);
        }
    }
    catch (...)
    {
        GADGET_THROW("Errors in hoSPIRIT2DTOperator<T>::set_forward_kernel(...) ... ");
    }
}

template <typename T>
void hoSPIRIT2DTOperator<T>::apply_forward_kernel(ARRAY_TYPE& x)
{
    try
    {
        size_t RO = x.get_size(0);
        size_t E1 = x.get_size(1);
        size_t srcCHA = x.get_size(2);
        size_t N = x.get_size(3);

        size_t dstCHA = this->forward_kernel_.get_size(3);
        size_t kernelN = this->forward_kernel_.get_size(4);

        long long n;
        for (n = 0; n < (long long)N; N++)
        {
            hoNDArray<T> currComplexIm(RO, E1, srcCHA, this->complexIm_.begin() + n*RO*E1*srcCHA);

            hoNDArray<T> curr_forward_kernel;

            if (n < (long long)kernelN)
            {
                curr_forward_kernel.create(RO, E1, srcCHA, dstCHA, this->forward_kernel_.begin() + n*RO*E1*srcCHA*dstCHA);
            }
            else
            {
                curr_forward_kernel.create(RO, E1, srcCHA, dstCHA, this->forward_kernel_.begin() + (kernelN - 1)*RO*E1*srcCHA*dstCHA);
            }

            Gadgetron::multiply(curr_forward_kernel, currComplexIm, res_after_apply_kernel_);

            hoNDArray<T> sumResCurr(RO, E1, 1, dstCHA, this->res_after_apply_kernel_sum_over_.begin() + n*RO*E1*dstCHA);
            Gadgetron::sum_over_dimension(this->res_after_apply_kernel_, sumResCurr, 2);
        }
    }
    catch(...)
    {
        GADGET_THROW("Errors in hoSPIRIT2DTOperator<T>::apply_forward_kernel(x) ... ");
    }
}

template <typename T>
void hoSPIRIT2DTOperator<T>::mult_M(ARRAY_TYPE* x, ARRAY_TYPE* y, bool accumulate)
{
    try
    {
        if (accumulate) 
        {
            kspace_dst_ = *y;
        }

        if(no_null_space_)
        {
            this->convert_to_image(*x, complexIm_);
        }
        else
        {
            // (G-I)Dc'x
            Gadgetron::multiply(unacquired_points_indicator_, *x, *y);

            // x to image domain
            this->convert_to_image(*y, complexIm_);
        }

        // apply kernel and sum
        this->apply_forward_kernel(complexIm_);

        // go back to kspace 
        this->convert_to_kspace(res_after_apply_kernel_sum_over_, *y);

        if(accumulate)
        {
            Gadgetron::add(kspace_dst_, *y, *y);
        }
    }
    catch (...)
    {
        GADGET_THROW("Errors in hoSPIRIT2DTOperator<T>::mult_M(...) ... ");
    }
}

template <typename T>
void hoSPIRIT2DTOperator<T>::apply_adjoint_kernel(ARRAY_TYPE& x)
{
    try
    {
        size_t RO = x.get_size(0);
        size_t E1 = x.get_size(1);
        size_t dstCHA = x.get_size(2);
        size_t N = x.get_size(3);

        size_t srcCHA = this->adjoint_kernel_.get_size(3);
        size_t kernelN = this->adjoint_kernel_.get_size(4);

        long long n;
        for (n = 0; n < (long long)N; N++)
        {
            hoNDArray<T> currComplexIm(RO, E1, dstCHA, this->complexIm_dst_.begin() + n*RO*E1*dstCHA);

            hoNDArray<T> curr_adjoint_kernel;

            if (n < (long long)kernelN)
            {
                curr_adjoint_kernel.create(RO, E1, dstCHA, srcCHA, this->adjoint_kernel_.begin() + n*RO*E1*dstCHA*srcCHA);
            }
            else
            {
                curr_adjoint_kernel.create(RO, E1, dstCHA, srcCHA, this->adjoint_kernel_.begin() + (kernelN - 1)*RO*E1*dstCHA*srcCHA);
            }

            Gadgetron::multiply(curr_adjoint_kernel, currComplexIm, res_after_apply_kernel_dst_);

            hoNDArray<T> sumResCurr(RO, E1, 1, srcCHA, this->res_after_apply_kernel_sum_over_dst_.begin() + n*RO*E1*srcCHA);
            Gadgetron::sum_over_dimension(this->res_after_apply_kernel_dst_, sumResCurr, 2);
        }
    }
    catch (...)
    {
        GADGET_THROW("Errors in hoSPIRIT2DTOperator<T>::apply_adjoint_kernel(x) ... ");
    }
}

template <typename T>
void hoSPIRIT2DTOperator<T>::mult_MH(ARRAY_TYPE* x, ARRAY_TYPE* y, bool accumulate)
{
    try
    {
        // Dc(G-I)'x

        if(accumulate)
        {
            kspace_dst_ = *y;
        }

        // x to image domain
        this->convert_to_image(*x, complexIm_dst_);

        // apply adjoint kernel and sum
        this->apply_adjoint_kernel(complexIm_dst_);

        // go back to kspace 
        this->convert_to_kspace(res_after_apply_kernel_sum_over_dst_, *y);

        if (!no_null_space_)
        {
            // apply Dc
            Gadgetron::multiply(unacquired_points_indicator_, *y, *y);
        }

        if (accumulate)
        {
            Gadgetron::add(kspace_dst_, *y, *y);
        }
    }
    catch (...)
    {
        GADGET_THROW("Errors in hoSPIRIT2DTOperator<T>::mult_MH(...) ... ");
    }
}

template <typename T>
void hoSPIRIT2DTOperator<T>::compute_righ_hand_side(const ARRAY_TYPE& x, ARRAY_TYPE& b)
{
    try
    {
        if (no_null_space_)
        {
            b.create(x.get_dimensions());
            Gadgetron::clear(b);
        }
        else
        {
            // non-symmetric rhs: -(G-I)D'x

            // need to be done for D'x, acquired points are already in place

            // x to image domain
            this->convert_to_image(x, complexIm_);

            // apply kernel and sum
            this->apply_forward_kernel(complexIm_);

            // go back to kspace 
            this->convert_to_kspace(res_after_apply_kernel_sum_over_, b);

            // multiply by -1
            Gadgetron::scal((typename realType<T>::Type)(-1.0), b);
        }
    }
    catch (...)
    {
        GADGET_THROW("Errors in hoSPIRIT2DTOperator<T>::compute_righ_hand_side(...) ... ");
    }
}

template <typename T>
void hoSPIRIT2DTOperator<T>::apply_adjoint_forward_kernel(ARRAY_TYPE& x)
{
    try
    {
        size_t RO = x.get_size(0);
        size_t E1 = x.get_size(1);
        size_t srcCHA = x.get_size(2);
        size_t N = x.get_size(3);

        GADGET_CHECK_THROW(this->adjoint_forward_kernel_.get_size(3)==srcCHA);
        size_t kernelN = this->adjoint_forward_kernel_.get_size(4);

        long long n;
        for (n = 0; n < (long long)N; N++)
        {
            hoNDArray<T> currComplexIm(RO, E1, srcCHA, this->complexIm_.begin() + n*RO*E1*srcCHA);

            hoNDArray<T> curr_adjoint_forward_kernel;

            if (n < (long long)kernelN)
            {
                curr_adjoint_forward_kernel.create(RO, E1, srcCHA, srcCHA, this->adjoint_forward_kernel_.begin() + n*RO*E1*srcCHA*srcCHA);
            }
            else
            {
                curr_adjoint_forward_kernel.create(RO, E1, srcCHA, srcCHA, this->adjoint_forward_kernel_.begin() + (kernelN - 1)*RO*E1*srcCHA*srcCHA);
            }

            Gadgetron::multiply(curr_adjoint_forward_kernel, currComplexIm, res_after_apply_kernel_dst_);

            hoNDArray<T> sumResCurr(RO, E1, 1, srcCHA, this->res_after_apply_kernel_sum_over_dst_.begin() + n*RO*E1*srcCHA);
            Gadgetron::sum_over_dimension(this->res_after_apply_kernel_dst_, sumResCurr, 2);
        }
    }
    catch (...)
    {
        GADGET_THROW("Errors in hoSPIRIT2DTOperator<T>::apply_adjoint_forward_kernel(x) ... ");
    }
}

template <typename T>
void hoSPIRIT2DTOperator<T>::gradient(ARRAY_TYPE* x, ARRAY_TYPE* g, bool accumulate)
{
    try
    {
        if (accumulate)
        {
            kspace_ = *g;
        }

        if (no_null_space_)
        {
            // gradient of L2 norm is 2*Dc*(G-I)'(G-I)x
            this->convert_to_image(*x, complexIm_);
        }
        else
        {
            // gradient of L2 norm is 2*Dc*(G-I)'(G-I)(D'y+Dc'x)

            // D'y+Dc'x
            Gadgetron::multiply(unacquired_points_indicator_, *x, kspace_);
            Gadgetron::add(acquired_points_, kspace_, kspace_);

            // x to image domain
            this->convert_to_image(kspace_, complexIm_);
        }

        // apply kernel and sum
        this->apply_adjoint_forward_kernel(complexIm_);

        // go back to kspace 
        this->convert_to_kspace(res_after_apply_kernel_sum_over_dst_, *g);

        // apply Dc
        Gadgetron::multiply(unacquired_points_indicator_, *g, *g);

        // multiply by 2
        Gadgetron::scal((typename realType<T>::Type)(2.0), *g);

        if (accumulate)
        {
            Gadgetron:add(kspace_, *g, *g);
        }
    }
    catch (...)
    {
        GADGET_THROW("Errors in hoSPIRIT2DTOperator<T>::gradient(...) ... ");
    }
}

template <typename T>
typename hoSPIRIT2DTOperator<T>::REAL hoSPIRIT2DTOperator<T>::magnitude(ARRAY_TYPE* x)
{
    try
    {
        if (no_null_space_)
        {
            // L2 norm ||(G-I)x||2
            this->convert_to_image(*x, complexIm_);
        }
        else
        {
            // L2 norm ||(G-I)(D'y+Dc'x)||2

            // D'y+Dc'x
            Gadgetron::multiply(unacquired_points_indicator_, *x, kspace_);
            Gadgetron::add(acquired_points_, kspace_, kspace_);

            // x to image domain
            this->convert_to_image(kspace_, complexIm_);
        }

        // apply kernel and sum
        this->apply_forward_kernel(complexIm_);

        // L2 norm
        T obj(0);
        Gadgetron::dotc(res_after_apply_kernel_sum_over_, res_after_apply_kernel_sum_over_, obj);

        return std::abs(obj);
    }
    catch (...)
    {
        GADGET_THROW("Errors in hoSPIRIT2DTOperator<T>::magnitude(...) ... ");
    }
}

template <typename T>
void hoSPIRIT2DTOperator<T>::convert_to_image(const hoNDArray<T>& x, hoNDArray<T>& im)
{
    try
    {
        if (this->use_non_centered_fft_)
        {
            Gadgetron::hoNDFFT<typename realType<T>::Type>::instance()->ifft2(x, im);
        }
        else
        {
            bool inSrc = complexIm_.dimensions_equal(&x);
            bool inDst = complexIm_dst_.dimensions_equal(&x);

            if (inSrc)
            {
                Gadgetron::hoNDFFT<typename realType<T>::Type>::instance()->ifft2c(x, im, complexIm_);
            }
            else if(inDst)
            {
                Gadgetron::hoNDFFT<typename realType<T>::Type>::instance()->ifft2c(x, im, complexIm_dst_);
            }
            else
            {
                complexIm_.create(x.get_dimensions());
                Gadgetron::hoNDFFT<typename realType<T>::Type>::instance()->ifft2c(x, im, complexIm_);
            }
        }
    }
    catch (...)
    {
        GADGET_THROW("Errors happened in hoSPIRIT2DTOperator<T>::convert_to_image(...) ... ");
    }
}

template <typename T>
void hoSPIRIT2DTOperator<T>::convert_to_kspace(const hoNDArray<T>& im, hoNDArray<T>& x)
{
    try
    {
        if (this->use_non_centered_fft_)
        {
            Gadgetron::hoNDFFT<typename realType<T>::Type>::instance()->fft2(im, x);
        }
        else
        {
            bool inSrc = kspace_.dimensions_equal(&x);
            bool inDst = kspace_dst_.dimensions_equal(&x);

            if (!kspace_.dimensions_equal(&im))
            {
                kspace_.create(im.get_dimensions());
            }

            if (inSrc)
            {
                Gadgetron::hoNDFFT<typename realType<T>::Type>::instance()->fft2c(im, x, kspace_);
            }
            else if(inDst)
            {
                Gadgetron::hoNDFFT<typename realType<T>::Type>::instance()->fft2c(im, x, kspace_dst_);
            }
            else
            {
                kspace_.create(x.get_dimensions());
                Gadgetron::hoNDFFT<typename realType<T>::Type>::instance()->fft2c(im, x, kspace_);
            }
        }
    }
    catch (...)
    {
        GADGET_THROW("Errors happened in hoSPIRIT2DTOperator<T>::convert_to_kspace(...) ... ");
    }
}

// ------------------------------------------------------------
// Instantiation
// ------------------------------------------------------------

template class EXPORTCPUOPERATOR hoSPIRIT2DTOperator< std::complex<float> >;
template class EXPORTCPUOPERATOR hoSPIRIT2DTOperator< std::complex<double> >;

}
