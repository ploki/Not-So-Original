/* 
 * Copyright (c) 2006-2011, Guillaume Gimenez <guillaume@blackmilk.fr>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of G.Gimenez nor the names of its contributors may
 *       be used to endorse or promote products derived from this software
 *       without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL G.Gimenez SA BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Authors:
 *     * Guillaume Gimenez <guillaume@blackmilk.fr>
 *
 */



/*
//convertion rgv <-> lch depuis ufraw et dcraw
static const double xyz_rgb[3][3] = {                  //XYZ from RGB
  { 0.412453, 0.357580, 0.180423 },
  { 0.212671, 0.715160, 0.072169 },
  { 0.019334, 0.119193, 0.950227 } };
static const double rgb_xyz[3][3] = {                   //RGB from XYZ
    { 3.24048, -1.53715, -0.498536 },
    { -0.969255, 1.87599, 0.0415559 },
    { 0.0556466, -0.204041, 1.05731 } };
*/

//sRGB D65 http://www.brucelindbloom.com/
static const double xyz_rgb[3][3] = {                  //XYZ from RGB
  { 0.4124564, 0.3575761, 0.1804375 },
  { 0.2126729, 0.7151522, 0.0721750 },
  { 0.0193339, 0.1191920, 0.9503041 } };
static const double rgb_xyz[3][3] = {                   //RGB from XYZ
    {  3.2404542, -1.5371385, -0.4985314 },
    { -0.9692660,  1.8760108,  0.0415560 },
    {  0.0556434, -0.2040259,  1.0572252 } };

/*
//sRGB D50 http://www.brucelindbloom.com/
static const double xyz_rgb[3][3] = {
  { 0.4360747, 0.3850649, 0.1430804 },
  { 0.2225045, 0.7168786, 0.0606169 },
  { 0.0139322, 0.0971045, 0.7141733 } };
static const double rgb_xyz[3][3] = {
  {  3.1338561, -1.6168667, -0.4906146 },
  { -0.9787684,  1.9161415,  0.0334540 },
  {  0.0719453, -0.2289914,  1.4052427 } };
*/


// Illuminant D50
//static const double illuminant[3] = { 0.96422, 1.00000,	0.82521 };

// Illuminent D65
static const double illuminant[3] = { 0.95047, 1.00000, 1.08883 };



/*
static void RGB_to_CIELab(int rgb[3], double lab[3])
{
	const double epsilon = 216./24389.,
		     kappa = 24389./27.;

	//RGB -> XYZ
	double xyz[3] = { 0., 0., 0. };

	for (int c=0; c<3; c++)
		for (int cc=0; cc<3; cc++)
			xyz[cc] += xyz_rgb[cc][c] * (double(rgb[c])/65535.);

	//XYZ -> CIELab

	double xr = xyz[0]/illuminant[0];
	double yr = xyz[1]/illuminant[1];
	double zr = xyz[2]/illuminant[2];


	double fx = xr > epsilon ? pow(xr,1./3.) : ( kappa * xr + 16. ) / 116.;
	double fy = yr > epsilon ? pow(yr,1./3.) : ( kappa * yr + 16. ) / 116.;
	double fz = zr > epsilon ? pow(zr,1./3.) : ( kappa * zr + 16. ) / 116.;

	lab[0] = 116. * fy - 16.;
	lab[1] = 500. * (fx - fy);
	lab[2] = 200. * (fy - fz);

}



static void CIELab_to_RGB(double lab[3], int rgb[3])
{
    int c, cc;
    double xyz[3], fx, fy, fz, xr, yr, zr, tmpf;
    const double epsilon =  216.L/24389.L,
		 kappa = 24389.L/27.L;

	// CIELab -> XYZ

    yr = (lab[0]<=kappa*epsilon) ?
        (lab[0]/kappa) : (pow((lab[0]+16.0)/116.0, 3.0));

//    fy = (yr<=epsilon) ? ((kappa*yr+16.0)/116.0) : ((lab[0]+16.0)/116.0);

	fy = ( lab[0]+16.)/116.;

    fz = fy - lab[2]/200.0;
    fx = lab[1]/500.0 + fy;

    zr = (pow(fz, 3.0)<=epsilon) ? ((116.0*fz-16.0)/kappa) : (pow(fz, 3.0));
    xr = (pow(fx, 3.0)<=epsilon) ? ((116.0*fx-16.0)/kappa) : (pow(fx, 3.0));

    xyz[0] = xr*illuminant[0];
    xyz[1] = yr*illuminant[1];
    xyz[2] = zr*illuminant[2];

	// XYZ -> RGB

    for (c=0; c<3; c++) {
        tmpf = 0;
        for (cc=0; cc<3; cc++)
            tmpf += rgb_xyz[c][cc] * xyz[cc];
        rgb[c] = ( tmpf<0?0:tmpf ) * 65535.0 + 0.5;
	if ( rgb[c] > signed(MaxRGB) ) rgb[c] = MaxRGB;

    }

}
*/
#define RGB_to_CIELab(rgb,lab) \
{ \
	const double epsilon = 216./24389., \
		     kappa = 24389./27.; \
        double xyz[3] = { 0., 0., 0. }; \
	xyz[0] += xyz_rgb[0][0] * (double(rgb[0])/65535.);\
	xyz[1] += xyz_rgb[1][0] * (double(rgb[0])/65535.);\
	xyz[2] += xyz_rgb[2][0] * (double(rgb[0])/65535.);\
	xyz[0] += xyz_rgb[0][1] * (double(rgb[1])/65535.);\
	xyz[1] += xyz_rgb[1][1] * (double(rgb[1])/65535.);\
	xyz[2] += xyz_rgb[2][1] * (double(rgb[1])/65535.);\
	xyz[0] += xyz_rgb[0][2] * (double(rgb[2])/65535.);\
	xyz[1] += xyz_rgb[1][2] * (double(rgb[2])/65535.);\
	xyz[2] += xyz_rgb[2][2] * (double(rgb[2])/65535.);\
	double xr = xyz[0]/illuminant[0]; \
	double yr = xyz[1]/illuminant[1]; \
	double zr = xyz[2]/illuminant[2]; \
	double fx = xr > epsilon ? pow(xr,1./3.) : ( kappa * xr + 16. ) / 116.;\
	double fy = yr > epsilon ? pow(yr,1./3.) : ( kappa * yr + 16. ) / 116.;\
	double fz = zr > epsilon ? pow(zr,1./3.) : ( kappa * zr + 16. ) / 116.;\
	lab[0] = 116. * fy - 16.;\
	lab[1] = 500. * (fx - fy);\
	lab[2] = 200. * (fy - fz);\
}



#define CIELab_to_RGB(lab,rgb) \
{ \
    double xyz[3], fx, fy, fz, xr, yr, zr, tmpf; \
    const double epsilon =  216.L/24389.L, \
		 kappa = 24389.L/27.L; \
    yr = (lab[0]<=kappa*epsilon) ? \
        (lab[0]/kappa) : (pow((lab[0]+16.0)/116.0, 3.0)); \
	fy = ( lab[0]+16.)/116.; \
    fz = fy - lab[2]/200.0; \
    fx = lab[1]/500.0 + fy; \
    zr = (pow(fz, 3.0)<=epsilon) ? ((116.0*fz-16.0)/kappa) : (pow(fz, 3.0)); \
    xr = (pow(fx, 3.0)<=epsilon) ? ((116.0*fx-16.0)/kappa) : (pow(fx, 3.0)); \
    xyz[0] = xr*illuminant[0]; \
    xyz[1] = yr*illuminant[1]; \
    xyz[2] = zr*illuminant[2]; \
        tmpf = 0; \
        tmpf += rgb_xyz[0][0] * xyz[0]; \
        tmpf += rgb_xyz[0][1] * xyz[1]; \
        tmpf += rgb_xyz[0][2] * xyz[2]; \
        rgb[0] = ( tmpf<0?0:tmpf ) * 65535.0 + 0.5; \
	if ( rgb[0] > signed(MaxRGB) ) rgb[0] = MaxRGB; \
        tmpf = 0; \
        tmpf += rgb_xyz[1][0] * xyz[0]; \
        tmpf += rgb_xyz[1][1] * xyz[1]; \
        tmpf += rgb_xyz[1][2] * xyz[2]; \
        rgb[1] = ( tmpf<0?0:tmpf ) * 65535.0 + 0.5; \
	if ( rgb[1] > signed(MaxRGB) ) rgb[1] = MaxRGB; \
        tmpf = 0; \
        tmpf += rgb_xyz[2][0] * xyz[0]; \
        tmpf += rgb_xyz[2][1] * xyz[1]; \
        tmpf += rgb_xyz[2][2] * xyz[2]; \
        rgb[2] = ( tmpf<0?0:tmpf ) * 65535.0 + 0.5; \
	if ( rgb[2] > signed(MaxRGB) ) rgb[2] = MaxRGB; \
}

static void lab_to_int(double lab[3], int lab16[3]) {
	lab16[0]=lab[0]*65535.L/100.L;
	lab16[1]=(lab[1]+120.L)*65535.L/240.L;
	lab16[2]=(lab[2]+120.L)*65535.L/240.L;
}

static void int_to_lab(int lab16[3], double lab[3]) {
	lab[0] = double(lab16[0])*100.L/65535.L;
	lab[1] = double(lab16[1])*240.L/65535.L-120;
	lab[2] = double(lab16[2])*240.L/65535.L-120;
}



static void RGB_to_CIELab16(int rgb[3], int lab16[3] ) {
	double lab[3];
	RGB_to_CIELab(rgb,lab);
	lab_to_int(lab,lab16);
}

static void CIELab16_to_RGB(int lab16[3], int rgb[3] ) {
	double lab[3];
	int_to_lab(lab16,lab);
	CIELab_to_RGB(lab,rgb);
}


/*
static double lab_linearize(double l) {
	const double epsilon =  216.L/24389.L, kappa = 24389.L/27.L;
	return (l<=kappa*epsilon) ?
		(l/kappa) : (pow((l+16.0)/116.0, 3.0));
}
static double lab_gammaize(double v) {
	const double epsilon =  216.L/24389.L, kappa = 24389.L/27.L;
	return 116. * ( v > epsilon ? pow(v,1./3.) : ( kappa * v + 16. ) / 116. ) - 16.;
}
*/

#define lab_linearize(l) ({ \
	const double epsilon =  216.L/24389.L, kappa = 24389.L/27.L; \
	double __ret = (l<=kappa*epsilon) ? \
		(l/kappa) : (pow((l+16.0)/116.0, 3.0)); \
__ret; })

#define lab_gammaize(v) ({ \
	const double epsilon =  216.L/24389.L, kappa = 24389.L/27.L; \
	double __ret = 116. * ( v > epsilon ? pow(v,1./3.) : ( kappa * v + 16. ) / 116. ) - 16.; \
__ret; })


/**
 * @param rgb int[3] linear [0..65535]
 * @param[out] lab double[3] Lab [0..1,-120..120,-120..120]
 */
#define RGB_to_LinearLab(rgb,lab) \
{ \
	const double epsilon = 216./24389., \
		     kappa = 24389./27.; \
	double xyz[3] = { 0., 0., 0. }; \
	xyz[0] += xyz_rgb[0][0] * (double(rgb[0])/65535.);\
	xyz[1] += xyz_rgb[1][0] * (double(rgb[0])/65535.);\
	xyz[2] += xyz_rgb[2][0] * (double(rgb[0])/65535.);\
	xyz[0] += xyz_rgb[0][1] * (double(rgb[1])/65535.);\
	xyz[1] += xyz_rgb[1][1] * (double(rgb[1])/65535.);\
	xyz[2] += xyz_rgb[2][1] * (double(rgb[1])/65535.);\
	xyz[0] += xyz_rgb[0][2] * (double(rgb[2])/65535.);\
	xyz[1] += xyz_rgb[1][2] * (double(rgb[2])/65535.);\
	xyz[2] += xyz_rgb[2][2] * (double(rgb[2])/65535.);\
	double xr = xyz[0]/illuminant[0]; \
	double yr = xyz[1]/illuminant[1]; \
	double zr = xyz[2]/illuminant[2]; \
	double fx = xr > epsilon ? pow(xr,1./3.) : ( kappa * xr + 16. ) / 116.; \
	double fy = yr > epsilon ? pow(yr,1./3.) : ( kappa * yr + 16. ) / 116.; \
	double fz = zr > epsilon ? pow(zr,1./3.) : ( kappa * zr + 16. ) / 116.; \
	lab[0] = yr; \
	lab[1] = 500. * (fx - fy); \
	lab[2] = 200. * (fy - fz); \
}



/**
 * @param lab double[3] Lab [0..1,-120..120,-120..120]
 * @param[out] rgb int[3] linear [0..65535]
 */
#define LinearLab_to_RGB(lab,rgb) \
{ \
    double xyz[3], fx, fy, fz, xr, yr, zr, tmpf; \
    const double epsilon =  216.L/24389.L, \
		 kappa = 24389.L/27.L; \
        yr = lab[0]; \
        fy = yr > epsilon ? pow(yr,1./3.) : ( kappa * yr + 16. ) / 116.; \
    fz = fy - lab[2]/200.0; \
    fx = lab[1]/500.0 + fy; \
    zr = (pow(fz, 3.0)<=epsilon) ? ((116.0*fz-16.0)/kappa) : (pow(fz, 3.0)); \
    xr = (pow(fx, 3.0)<=epsilon) ? ((116.0*fx-16.0)/kappa) : (pow(fx, 3.0)); \
    xyz[0] = xr*illuminant[0]; \
    xyz[1] = yr*illuminant[1]; \
    xyz[2] = zr*illuminant[2]; \
        tmpf = 0; \
        tmpf += rgb_xyz[0][0] * xyz[0]; \
        tmpf += rgb_xyz[0][1] * xyz[1]; \
        tmpf += rgb_xyz[0][2] * xyz[2]; \
        rgb[0] = ( tmpf<0?0:tmpf ) * 65535.0 + 0.5; \
	if ( rgb[0] > signed(MaxRGB) ) rgb[0] = MaxRGB; \
        tmpf = 0; \
        tmpf += rgb_xyz[1][0] * xyz[0]; \
        tmpf += rgb_xyz[1][1] * xyz[1]; \
        tmpf += rgb_xyz[1][2] * xyz[2]; \
        rgb[1] = ( tmpf<0?0:tmpf ) * 65535.0 + 0.5; \
	if ( rgb[1] > signed(MaxRGB) ) rgb[1] = MaxRGB; \
        tmpf = 0; \
        tmpf += rgb_xyz[2][0] * xyz[0]; \
        tmpf += rgb_xyz[2][1] * xyz[1]; \
        tmpf += rgb_xyz[2][2] * xyz[2]; \
        rgb[2] = ( tmpf<0?0:tmpf ) * 65535.0 + 0.5; \
	if ( rgb[2] > signed(MaxRGB) ) rgb[2] = MaxRGB; \
}



