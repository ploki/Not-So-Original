	/*
CREATE TABLE gallery_process (
    filename VARCHAR, -- chemin réel du fichier url_encodé
    order   INTEGER,  -- numero d'operation dans le process
    operator VARCHAR, -- nom de l'operateur
    params VARCHAR    -- les parametres
)
*/
#include <Magick++.h>
#include "CIELab.H"
#include "Profile.H"

using namespace Magick;
namespace op {

static inline void Labize(Image &image, Image &labImage);
static inline void unLabize(Image &image, Image &labImage);

#define LAB_G	3.0L
//#define LAB_N	0.159997L
#define LAB_N 0.008856L
//faux 2.64388L

#define SRGB_G	2.4L
#define SRGB_N	0.00304L
//8.36171L

#define BT709_G 2.222L
#define BT709_N 0.018L
//5.79586L

#define LIN_G	1.0L
#define LIN_N	0.0L


	/*
		pour un gamma à deux param : pente p + gamma g
		on a :
			d/dx (1+a)*x^(1/g)-a = d/dx p*x + k

			(a+1)*x^(1/g-1) / g = p

		donc la partie linéaire et la partie nonlinéaire se croisent pour :
			x0=(p*g/(a+1))^(1/(1/g-1))

		en revanche, on peut déterminer a et p par rapport à x0 pour que la pente
		soit linéaire (f(0)=0) pour les zones de basses lumières inférieures à x0
		à l'aide de y = f'(x0)*(x-x0)+f(x0)

		Y = A*x+B
		A = (a+1)*x0^(1/g)/(g*x0)
		B = (a+1)(g-1)*x0^(1/g)/g - a = 0
		-> a = - (g-1)*x0^(1/g)/((g-1)*x0^(1/g)-g)

		-> p = f'(x0) = (a+1)*x0^(1/g)/(g*x0)
		->

		concernant la réciproque :
			d/dx ((x+a)/(a+1))^g = d/dx x/p
			g*((x+a)/(a+1))^g/(x+a) = 1/p

		il faut obtenir x0=(1+a)*(x0 demandé)^(1/g)-a
		A= on s'en moque
		B= ((x0+a)/(a+1))^g - g*x0((x0+a)/(a+1))^g/(x0+a) = 0
		-> a = (g-1)*x0 (et aussi a = -x0 )
		-> p = f'(x0) = g*((x0+a)/(a+1))^g/(x0+a)
	*/

	static inline void iGamma(Image& image, double gamma, double x0, bool invert=false, int channels=3) {
	        //semble optimale
                //Profile prof("iGamma");
		if ( gamma == 1.0L ) return;
		double a = - ( gamma - 1.L)*pow(x0,1.L/gamma)/((gamma-1.L)*pow(x0,1.L/gamma)-gamma);
		double p=0;
		if ( invert ) {
			//recalcul de x0 et a
			x0=(1.L+a)*pow(x0,1.L/gamma)-a;
			a=(gamma-1.L)*x0;
			p=gamma*pow((x0+a)/(a+1.L),gamma)/(x0+a);
		}
		else
			p=(a+1.L)*pow(x0,1.L/gamma)/(gamma*x0);

		unsigned int lut[MaxRGB+1];
		#pragma omp parallel for
		for ( int i = 0 ; i <= signed(MaxRGB) ; ++i ) {
			double xx= double(i)/double(MaxRGB);
			if ( xx > x0 ) {
				if ( invert ) {
					lut[i]=pow(((xx+a)/(a+1.L)),gamma)*MaxRGB;
				}
				else {
					lut[i]=((1.L+a)*pow(xx,(1.L/gamma))-a)*MaxRGB;
				}
			}
			else {
				lut[i]=p*xx*double(MaxRGB);
			}
		}
		#pragma omp barrier



		int h = image.rows(),
			w = image.columns();
		image.modifyImage();
		Pixels pixel_cache(image);

	#pragma omp parallel for
		for ( int y = 0 ; y < h ; ++y ) {
			PixelPacket *pixels = pixel_cache.get(0,y,w,1);
			if ( !pixels ) continue;
			for ( int x = 0 ; x < w ; ++x ) {
				pixels[x].red=lut[pixels[x].red];
				pixels[x].green=lut[pixels[x].green];
				pixels[x].blue=lut[pixels[x].blue];
			}
		}
	#pragma omp barrier
		pixel_cache.sync();

	}


  static inline Image removeHotPixels(Image& input, double delta=0.5L, bool aggressive=false, bool naive=false) {
                        Profile prof("removeHotPixels");
			Image image=input;

			image.modifyImage();

			Pixels output_cache(image),
				input_cache(input);

			int w = image.columns(),
				h = image.rows();

			#pragma omp parallel for
			for ( int y = 1 ; y < h-1 ; ++y ) {
				PixelPacket *output_pixels = output_cache.get(0,y,w,1);
				PixelPacket *input_pixels[3];
				input_pixels[0] = input_cache.get(0,y-1,w,1);
				input_pixels[1] = input_cache.get(0,y,w,1);
				input_pixels[2] = input_cache.get(0,y+1,w,1);

				for ( int x = 1 ; x < w-1 ; ++x ) {
					int max_rgb[3]={0,0,0};
					int min_rgb[3]={MaxRGB,MaxRGB,MaxRGB};
					int sum_rgb[3]={0,0,0};
					int rgb[3];

					rgb[0]=input_pixels[1][x].red;
					rgb[1]=input_pixels[1][x].green;
					rgb[2]=input_pixels[1][x].blue;
/*
					for ( int j = -1 ; j < 2 ; ++j ) for ( int i = -1 ; i < 2 ; ++i ) {
						if ( i == 0 && j == 0 ) continue;

						int nrgb[3];

						nrgb[0]=input_pixels[j+1][x+i].red;
						nrgb[1]=input_pixels[j+1][x+i].green;
						nrgb[2]=input_pixels[j+1][x+i].blue;
						for ( int q = 0 ; q < 3 ; ++q ) {
							if ( !naive ) {
								if ( nrgb[q] > max_rgb[q] ) max_rgb[q] = nrgb[q];
								if ( nrgb[q] < min_rgb[q] ) min_rgb[q] = nrgb[q];
							}
							sum_rgb[q]+=nrgb[q];
						}
					}

					for ( int q = 0 ; q < 3 ; ++q ) {
						int other_channels = 0;
						if ( !aggressive )
							other_channels = (rgb[(q-1)%3]+rgb[(q+1)%3])/2;
						if ( naive ) {
							sum_rgb[q]/=8;
						}
						else {
							sum_rgb[q]-=(max_rgb[q]+min_rgb[q]);
							sum_rgb[q]/=6;
						}
						if ( sum_rgb[q]*delta < rgb[q] && ( aggressive || other_channels*delta < rgb[q] ) ) rgb[q]=sum_rgb[q];
						if ( sum_rgb[q]/delta > rgb[q] && ( aggressive || other_channels/delta > rgb[q] ) ) rgb[q]=sum_rgb[q];
					}
*/

//loops unroll

					#define color_op_naive(q) \
							sum_rgb[q]+=nrgb[q];

					#define color_op(q) \
							if ( nrgb[q] > max_rgb[q] ) max_rgb[q] = nrgb[q]; \
							if ( nrgb[q] < min_rgb[q] ) min_rgb[q] = nrgb[q]; \
							sum_rgb[q]+=nrgb[q];


					#define loop_code_naive(j,i) \
						nrgb[0]=input_pixels[j+1][x+i].red; \
						nrgb[1]=input_pixels[j+1][x+i].green; \
						nrgb[2]=input_pixels[j+1][x+i].blue; \
						color_op_naive(0); color_op_naive(1); color_op_naive(2)

					#define loop_code(j,i) \
						nrgb[0]=input_pixels[j+1][x+i].red; \
						nrgb[1]=input_pixels[j+1][x+i].green; \
						nrgb[2]=input_pixels[j+1][x+i].blue; \
						color_op(0); color_op(1); color_op(2)


					#define color_op2(q) \
						other_channels = (rgb[(q+2)%3]+rgb[(q+1)%3])/2; \
						sum_rgb[q]-=(max_rgb[q]+min_rgb[q]); \
						sum_rgb[q]/=6; \
						if ( sum_rgb[q]*delta < rgb[q] && other_channels*delta < rgb[q] ) rgb[q]=sum_rgb[q]; \
						if ( sum_rgb[q]/delta > rgb[q] && other_channels/delta > rgb[q] ) rgb[q]=sum_rgb[q];

					#define color_op2_naive(q) \
						other_channels = (rgb[(q+2)%3]+rgb[(q+1)%3])/2; \
						sum_rgb[q]/=8; \
						if ( sum_rgb[q]*delta < rgb[q] && other_channels*delta < rgb[q] ) rgb[q]=sum_rgb[q]; \
						if ( sum_rgb[q]/delta > rgb[q] && other_channels/delta > rgb[q] ) rgb[q]=sum_rgb[q];


					#define color_op2_aggressive(q) \
						sum_rgb[q]-=(max_rgb[q]+min_rgb[q]); \
						sum_rgb[q]/=6; \
						if ( sum_rgb[q]*delta < rgb[q] ) rgb[q]=sum_rgb[q]; \
						if ( sum_rgb[q]/delta > rgb[q] ) rgb[q]=sum_rgb[q];

					#define color_op2_naive_aggressive(q) \
						sum_rgb[q]/=8; \
						if ( sum_rgb[q]*delta < rgb[q] ) rgb[q]=sum_rgb[q]; \
						if ( sum_rgb[q]/delta > rgb[q] ) rgb[q]=sum_rgb[q];

					int nrgb[3];
					int other_channels=0;

					if ( naive ) {
						loop_code_naive(-1,-1); loop_code_naive(-1, 0); loop_code_naive(-1, 1);
						loop_code_naive( 0,-1);                         loop_code_naive( 0, 1);
						loop_code_naive( 1,-1); loop_code_naive( 1, 0); loop_code_naive( 1, 1);

						if ( aggressive ) {
							color_op2_naive_aggressive(0); color_op2_naive_aggressive(1); color_op2_naive_aggressive(2);
						}
						else {
							color_op2_naive(0); color_op2_naive(1); color_op2_naive(2);
						}
					}
					else {
						loop_code(-1,-1); loop_code(-1, 0); loop_code(-1, 1);
						loop_code( 0,-1);                   loop_code( 0, 1);
						loop_code( 1,-1); loop_code( 1, 0); loop_code( 1, 1);

						if ( aggressive ) {
							color_op2_aggressive(0); color_op2_aggressive(1); color_op2_aggressive(2);
						}
						else {
							color_op2(0); color_op2(1); color_op2(2);
						}
					}


					
					#undef loop_code
					#undef loop_code_naive
					#undef color_op
					#undef color_op_naive
					#undef color_op2
					#undef color_op2_naive
					#undef color_op2_aggressive
					#undef color_op2_naive_aggressive
//end of loops unroll


					output_pixels[x].red=rgb[0];
					output_pixels[x].green=rgb[1];
					output_pixels[x].blue=rgb[2];

				}
			}
			#pragma omp barrier
			input_cache.sync();
			output_cache.sync();

			return image;
	}


    DECLARE_EXCEPTION(OperatorException,raii::Exception);
    DECLARE_EXCEPTION(ProcessException,raii::Exception);

	class ImageInfo {
		public:
		double gamma;
		double non_linear_range;
		int iso_speed;
		double shutter_speed,aperture,focal;
		double daylight_multipliers[3],camera_multipliers[3];
		String camera;
		String timestamp;
		String filterPattern;
		ImageInfo(double g,double n) : gamma(g), non_linear_range(n), iso_speed(-1),
			shutter_speed(-1), aperture(-1), focal(-1),
			daylight_multipliers(), camera_multipliers(),
			camera("Unknown"), timestamp("Unknown"), filterPattern("") {
			for (int i=0 ; i < 3 ; ++i ) {
				daylight_multipliers[i]=1.;
				camera_multipliers[i]=1.;
			}
		}
		void probeFile(const String& path) {
/*
	Info à lire
Camera: NIKON D700
Timestamp: Wed Jun 18 22:54:47 2008
ISO speed: 12800
Shutter: 1/200.0 sec
Aperture: f/10.0
Focal length: 200.0 mm
Daylight multipliers: 2.064871 0.932310 1.112389
Camera multipliers: 1.503906 1.000000 1.648438 0.000000

*/
			FILE *f=popen(("dcraw -i -v \""+path+"\"").c_str(),"r");
			if ( f ) {
				char buf[1024];
				while ( fgets(buf,1024,f) ) {
					String str=buf;
					Vector<String> elem = str.explode(" ");

					if ( str.matches("^ISO speed: ") ) {
						iso_speed = elem[2].toi();
					}
					else if ( str.matches("Shutter: ") ) {
						Vector<String> sh = elem[1].explode("/");
						shutter_speed = sh[0].tof();
						if ( sh.size() == 2 ) shutter_speed/=sh[1].tof();
					}
					else if ( str.matches("Aperture: ") ) {
						Vector<String> sh = elem[1].explode("/");
						aperture = sh[1].tof();
					}
					else if ( str.matches("Focal length: ") ) {
						focal = elem[2].tof();
					}
					else if ( str.matches("Daylight multipliers: ") ) {
						daylight_multipliers[0] = elem[2].tof();
						daylight_multipliers[1] = elem[3].tof();
						daylight_multipliers[2] = elem[4].tof();
						if ( elem.size() == 6 ) {
							double div = elem[5].tof();
							if ( div != 0.L ) {
								daylight_multipliers[0]/=div;
								daylight_multipliers[1]/=div;
								daylight_multipliers[2]/=div;
							}
						}
					}
					else if ( str.matches("Camera multipliers: ") ) {
						camera_multipliers[0] = elem[2].tof();
						camera_multipliers[1] = elem[3].tof();
						camera_multipliers[2] = elem[4].tof();
						if ( elem.size() == 6 ) {
							double div = elem[5].tof();
							if ( div != 0.L ) {
								camera_multipliers[0]/=div;
								camera_multipliers[1]/=div;
								camera_multipliers[2]/=div;
							}
						}
					}
					else if ( str.matches("Camera: ") ) {
						camera=buf+8;
					}
					else if ( str.matches("TimeStamp: ") ) {
						timestamp=buf+11;
					}
					else if ( str.matches("Filter pattern: ") ) {
					        filterPattern=buf+16;
					}

				}
			
			}
			pclose(f);
		}
	
	};

    class Operator : public Object {
        protected:
        bool upToDate;
        public:
        String name;
        Map<String,String> param;
        bool enabled;
        int id;
        int serial;
        Image image;
        ptr<Operator> lastOp;
	ImageInfo imageInfo;

	Image curve;

        public:
        Operator(const String& str,int i,ptr<Operator> op)
                : upToDate(false), name(str), param(), enabled(true),
                  id(i), serial(0), image(), lastOp(op), imageInfo(LIN_G,LIN_N),curve(1,1,"RGBA",IntegerPixel,"\0\0\0\0\0\0\0\0" "\0\0\0\0\0\0\0\0" /* "\0\0\0\0\0\0\0\0" "\0\0\0\0\0\0\0\0" */) {

            mod("maxsize","0");
	}
        virtual ~Operator() {}

        void enable() { upToDate=false; enabled=true; }
        void disable() { upToDate=false; enabled=false; }
        bool isEnabled() { return enabled; }
        int getId() { return id; }
        int getSerial() { return serial; }

        virtual  Image* propagate() {
                return &image;
        }
        virtual Image * getImage() {
                return &image;
        }
	virtual Image * getCurve() {
		return &curve;
	}

        virtual void doJob()=0;

        bool apply(bool force) {
                if ( ! upToDate || force ) {
                        //Logger log("Process");
                        //log(String("applying operator ")+name+" "+itostring(id));
			if ( lastOp ) {
				image =* (lastOp->propagate());
				curve =* (lastOp->getCurve());
				imageInfo = (lastOp->imageInfo);
			}
                        if ( isEnabled() )
                                doJob();
                        upToDate = true;
                        return true;
                }
                return false;
        }

        String partialName() { return String("/operator/_")+name+".csp"; }
        String getName() { return name; }

        virtual bool isParamNameValid(const String& name) { return true; }

        void mod(const String& name, const String& value) {
            upToDate=false;
            ++serial;
            if ( isParamNameValid(name) )
                param[name]=value;
        }
        void outDate() { upToDate=false; }
        String mod(const String& name) {
            if ( isParamNameValid(name) )
                return param[name];
            else return "";
        }
	int getMaxSize() {
		if ( mod("maxsize").empty() || mod("maxsize").toi() == 0 ) {
			if ( lastOp )
				return lastOp->getMaxSize();
			else
				return 0;
		}
		return mod("maxsize").toi();
	}
        void resize(Magick::Image &img) {
		int maxsize=getMaxSize();
            if ( maxsize  != 0 ) {
                Magick::Geometry geo = img.size();
                int     w=geo.width(),
                        h=geo.height(),
                        W,H;
                double coef=1.;
                if ( w < h )
                        coef=((double)maxsize)/((double)h);
                else
                        coef=((double)maxsize)/((double)w);
                W=(int)(w*coef);
                H=(int)(h*coef);
                img.scale(Geometry(W,H));

            }
        }

    };

    class GenericLoad : public Operator {
        Image cropped;
        Image resized;
        public:
        GenericLoad(bool raw,const String& root, const String& tmp, const String& filename)
        : Operator(raw?"loadraw":"load",0,NULL), cropped(),resized() {
            mod("root",root);
            mod("tmp",tmp);
            mod("filename",filename);
	    mod("autocrop","");
            mod("crop_x1","0");
            mod("crop_y1","0");
            mod("crop_x2","0");
            mod("crop_y2","0");
            mod("rotate","0");
            mod("zoom","100");
            serial=0;
        }
        Image* propagate() {



                cropped=image;


                double rotate=mod("rotate").tof();
                cropped.rotate(rotate);
                Magick::Geometry geo = cropped.size();
                int w=geo.width(),
                    h=geo.height();
                //% de l'image et non pas % des dimensions
                double zoom=sqrt(mod("zoom").tof()/100);
                if ( zoom > 0 && zoom < 100 ) {
                        cropped.crop(Geometry(zoom*w,zoom*h,(w-zoom*w)/2,(h-zoom*h)/2));
                        geo = cropped.size();
        		w=geo.width();
        	        h=geo.height();
        	}

                
		String autocrop = mod("autocrop");


		if ( autocrop == "reset" ) {
			mod("crop_x1","0");mod("crop_y1","0");
			mod("crop_x2","0");mod("crop_y2","0");
			mod("autocrop","");
                }
		else if ( autocrop == "center" ) {
			if ( w>h ) {
				mod("crop_x1",itostring((w-h)/2));mod("crop_y1","0");
				mod("crop_x2",itostring(w-(w-h)/2));mod("crop_y2",itostring(h));
				cropped.crop(Geometry(h,h,(w-h)/2,0));
			}
			else {
				mod("crop_y1",itostring((h-w)/2));mod("crop_x1","0");
				mod("crop_y2",itostring(w-(w-h)/2));mod("crop_x2",itostring(w));
				cropped.crop(Geometry(w,w,0,(h-w)/2));
			}
		}
		else if ( autocrop == "left/top" ) {
			int l = w>h?h:w;
			mod("crop_x1","0");mod("crop_y1","0");
			mod("crop_x2",itostring(l));mod("crop_y2",itostring(l));
			cropped.crop(Geometry(l,l,0,0));
		}
		else if ( autocrop == "right/bottom" ) {
			int l = w>h?h:w;
			mod("crop_x1",itostring(w-l));mod("crop_y1",itostring(h-l));
			mod("crop_x2",itostring(w));mod("crop_y2",itostring(h));
			cropped.crop(Geometry(l,l,w-l,h-l));
		}
		else if ( autocrop == "4/3-center" ) {
			int l = w>h?h:w;
			int L = l*4/3;
			if ( w>h ) {
				mod("crop_x1",itostring((w-L)/2));mod("crop_y1","0");
				mod("crop_x2",itostring(w-(w-L)/2));mod("crop_y2",itostring(h));
				cropped.crop(Geometry(L,l,(w-L)/2,0));
			}
			else {
				mod("crop_y1",itostring((h-L)/2));mod("crop_x1","0");
				mod("crop_y2",itostring(w-(w-L)/2));mod("crop_x2",itostring(w));
				cropped.crop(Geometry(l,L,0,(h-L)/2));
			}
		}
		else if ( autocrop == "4/3-left/top" ) {
			int l = w>h?h:w;
			int L = l*4/3;
			if ( w>h ) {
				mod("crop_x1","0");mod("crop_y1","0");
				mod("crop_x2",itostring(L));mod("crop_y2",itostring(l));
				cropped.crop(Geometry(L,l,0,0));
			}
			else {
				mod("crop_x1","0");mod("crop_y1","0");
				mod("crop_x2",itostring(l));mod("crop_y2",itostring(L));
				cropped.crop(Geometry(l,L,0,0));
			}
		}
		else if ( autocrop == "4/3-right/bottom" ) {
			int l = w>h?h:w;
			int L = l*4/3;
			if ( w>h ) {
				mod("crop_x1",itostring(w-L));mod("crop_y1",itostring(h-l));
				mod("crop_x2",itostring(w));mod("crop_y2",itostring(h));
				cropped.crop(Geometry(L,l,w-L,h-l));
			}
			else {
				mod("crop_x1",itostring(w-l));mod("crop_y1",itostring(h-L));
				mod("crop_x2",itostring(w));mod("crop_y2",itostring(h));
				cropped.crop(Geometry(l,L,w-l,h-L));
			}
		}
		else /* if ( autocrop.empty() ) */ {
                int     x1=mod("crop_x1").toi(),
                        y1=mod("crop_y1").toi(),
                        x2=mod("crop_x2").toi(),
                        y2=mod("crop_y2").toi();
			if ( ! (x1 == 0 && x2 == 0 && y1 == 0 && y2 == 0 ) ) {
				if ( x1 > x2 ) { int t=x1; x1=x2; x2=t; }
				if ( y1 > y2 ) { int t=y1; y1=y2; y2=t; }
				cropped.crop(Geometry(x2-x1,y2-y1,x1,y1));
			}
		}

                resize(cropped);
                return &cropped;
        }
        Image* getImage() {
                resized=image;
                resize(resized);
                return &resized;
        }
   };

    class Load : public GenericLoad {
        public:
        Load(const String& root, const String& tmp, const String& fn) : GenericLoad(false,root, tmp,fn) {
            mod("discrete","0");
            mod("linear","0");
            serial=0;
	}

        void doJob() {
                try{
                    image.read(mod("root") + mod("filename"));
                }
                catch(...){
                    throw raii::Exception("ici");
                }

		curve.size("65536x1");
        	Pixels curve_cache(curve);
		PixelPacket *pixels = curve_cache.get(0,0,65536,1);
		if ( mod("discrete").toi() ) {
			double discrete=mod("discrete").tof();
			for(int i=0; i < 65536 ; ++i) {
				unsigned int c = pow(2., 16. - floor( -discrete*log(double(i)/MaxRGB)/log(2) )/discrete );
				if (c > MaxRGB) c=MaxRGB;
				pixels[i].red=pixels[i].green=pixels[i].blue=c;
			}
		}
		else
			for(int i=0; i < 65536 ; ++i)
				pixels[i].red=pixels[i].green=pixels[i].blue=i;
		curve_cache.sync();
		if ( mod("linear").toi() ) {
			imageInfo.gamma=LIN_G;
			imageInfo.non_linear_range=LIN_N;
		}
		else {
			//sRGB
			imageInfo.gamma=SRGB_G;
			imageInfo.non_linear_range=SRGB_N;

		}
		iGamma(curve,imageInfo.gamma,imageInfo.non_linear_range,false);

		imageInfo.probeFile(mod("root")+mod("filename"));
        }
    };

    class LoadRaw : public GenericLoad {
        public:
        LoadRaw(const String& root, const String& tmp, const String& fn) : GenericLoad(true,root,tmp,fn) {
              //mod("autWB","0");//(false),
              //mod("cameraWB","1");//(true),
              mod("whitebalance","2");
              mod("brightness","1.0");//(1.0),
              mod("blackpoint","");//(0),
              mod("highlights","0");//(0),
              mod("quality","1");//(0),
              mod("halfSize","0");//(0),
              mod("fourColors","0");//(false),
              mod("sigmaDomain","2");//(2),
              mod("sigmaRange","4");//(4)
              mod("16bit","0");
              mod("temperature","6500");
              mod("tint","1.0");
              mod("red_mag","1.000");
              mod("blue_mag","1.000");
              mod("noise","");

		mod("hp_enabled","0"); //seuil ISO
		mod("hp_delta","1");
		mod("hp_aggressive","0"); //seuil ISO
		mod("hp_naive","0");

		mod("discrete","0");
              serial=0;
        }

        void Temperature_to_RGB(double T, double RGB[3]) {


                const double XYZ_to_RGB[3][3] = {
                                { 3.24071,  -0.969258,  0.0556352 },
                                {-1.53726,  1.87599,    -0.203996 },
                                {-0.498571, 0.0415557,  1.05707 } };

                int c;
                double xD, yD, X, Y, Z, max;
                // Fit for CIE Daylight illuminant
                if (T<= 4000) {
                        xD = 0.27475e9/(T*T*T) - 0.98598e6/(T*T) + 1.17444e3/T + 0.145986;
                } else if (T<= 7000) {
                        xD = -4.6070e9/(T*T*T) + 2.9678e6/(T*T) + 0.09911e3/T + 0.244063;
                } else {
                        xD = -2.0064e9/(T*T*T) + 1.9018e6/(T*T) + 0.24748e3/T + 0.237040;
                }
                yD = -3*xD*xD + 2.87*xD - 0.275;

                // Fit for Blackbody using CIE standard observer function at 2 degrees
                //xD = -1.8596e9/(T*T*T) + 1.37686e6/(T*T) + 0.360496e3/T + 0.232632;
                //yD = -2.6046*xD*xD + 2.6106*xD - 0.239156;

                // Fit for Blackbody using CIE standard observer function at 10 degrees
                //xD = -1.98883e9/(T*T*T) + 1.45155e6/(T*T) + 0.364774e3/T + 0.231136;
                //yD = -2.35563*xD*xD + 2.39688*xD - 0.196035;

                X = xD/yD;
                Y = 1;
                Z = (1-xD-yD)/yD;
                max = 0;
                for (c=0; c<3; c++) {
                        RGB[c] = X*XYZ_to_RGB[0][c] + Y*XYZ_to_RGB[1][c] + Z*XYZ_to_RGB[2][c];
                        if (RGB[c]>max) max = RGB[c];
                }
                for (c=0; c<3; c++) RGB[c] = RGB[c]/max;
        }
        template<typename t> t clamp(t v,t min, t max) {
                if ( v < min )
                        return min;
                else if ( v > max )
                        return max;
                else
                        return v;
        }

        void doJob() {
                prepareTmpDir(mod("root"),mod("tmp"),mod("filename"));
                String tfile = mod("tmp") + "/processed" + mod("root") + mod("filename") + ".ppm";

            StringStream ss;
            ss << "dcraw -c ";
            //if (mod("autoWB").toi()) ss << "-a ";
            //if (mod("cameraWB").toi()) ss << "-w ";
            int wb=mod("whitebalance").toi();

		imageInfo.probeFile(mod("root")+mod("filename"));

            switch (wb) {
                case -1: {
                        double RGB[3];
                        double temperature = clamp<double>(mod("temperature").tof(),2000,12000);
                        mod("temperature",ftostring(temperature));
                        Temperature_to_RGB(temperature,RGB);

                        RGB[1]= RGB[1] / mod("tint").tof();

                        //Daylight multipliers
                        RGB[0]= imageInfo.daylight_multipliers[0] /RGB[0];
                        RGB[1]= imageInfo.daylight_multipliers[1] /RGB[1];
                        RGB[2]= imageInfo.daylight_multipliers[2] /RGB[2];


                        double minmul=RGB[0];
                        if ( minmul > RGB[1] ) minmul=RGB[1];
                        if ( minmul > RGB[2] ) minmul=RGB[2];
                        RGB[0]/=minmul;
                        RGB[1]/=minmul;
                        RGB[2]/=minmul;

                        ss << "-r " << RGB[0] <<" "<< RGB[1] << " "<< RGB[2] << " 1 ";
                        break;
                }
		default:
			{
				Connection conn;
				ResultSet rs= conn.query("SELECT op,rx,gx,bx FROM gallery_white_balance WHERE id='"+itostring(wb)+"'");
				if ( rs.next() ) {
					if ( rs["op"] == "-r" ) {
						double RGB[3];
			                        RGB[0]= imageInfo.daylight_multipliers[0] * rs["rx"].tof();
			                        RGB[1]= imageInfo.daylight_multipliers[1] * rs["gx"].tof();
			                        RGB[2]= imageInfo.daylight_multipliers[2] * rs["bx"].tof();
						
			                        double minmul=RGB[0];
			                        if ( minmul > RGB[1] ) minmul=RGB[1];
			                        if ( minmul > RGB[2] ) minmul=RGB[2];
			                        RGB[0]/=minmul;
			                        RGB[1]/=minmul;
			                        RGB[2]/=minmul;
	
			                        ss << rs["op"] << " " << RGB[0] <<" "<< RGB[1] << " "<< RGB[2] << " 1 ";
					}
					else {
						ss << rs["op"] << " ";
					}

				}
			}
            }
            if ( ! mod("noise").empty() )
                ss << "-n " <<mod("noise").toi()<<" ";
            ss << "-b "<<mod("brightness").tof()<<" ";
            if ( ! mod("blackpoint").empty() )
                ss << "-k "<<mod("blackpoint").toi()<<" ";
            ss << "-q "<<mod("quality").toi()<<" ";

                switch ( mod("halfSize").toi() ) {
                        case 0: //full size dcraw interpolation
                                //nop
                                break;
                        default:
                        case 1: //halfsize
                                ss << "-h ";
                                break;
                        case 2: //rawdata
                        case 3: //internal interpolation
                                ss << "-d ";
                                break;
                }

            //if ( mod("halfSize").toi() == 0 ) {
            if (mod("fourColors").toi()) ss<<"-f ";
            //}
            ss << "-H "<<mod("highlights").toi()<<" ";
            
            switch ( mod("16bit").toi() ) {
                        case 1: //16bit linear
                                ss << "-6 -W -g 1 1 ";
                                imageInfo.gamma=LIN_G;
                                imageInfo.non_linear_range=LIN_N;
                                break;
                        case 2: //16bit IUT BT.709
                                ss << "-6 -W -g 2.222 4.5 ";
                                imageInfo.gamma=BT709_G;
                                imageInfo.non_linear_range=BT709_N;
                                break;
                        case 3: //16bit sRGB
                                ss << "-6 -W -g 2.4 12.8 ";
                                imageInfo.gamma=SRGB_G;
                                imageInfo.non_linear_range=SRGB_N;
                                break;
                        case 4: //16bit IUT BT.709 (auto WP)
                                ss << "-6 -g 2.222 4.5 ";
                                imageInfo.gamma=BT709_G;
                                imageInfo.non_linear_range=BT709_N;
                                break;
                        case 5: //16bit sRGB (auto WP)
                                ss << "-6 -g 2.4 12.8 ";
                                imageInfo.gamma=SRGB_G;
                                imageInfo.non_linear_range=SRGB_N;
                                break;
                        case 6: //8bit sRGB (auto WP)
                                ss << "-g 2.4 12.8 ";
                                imageInfo.gamma=SRGB_G;
                                imageInfo.non_linear_range=SRGB_N;
                                break;
                        case 7: //8bit linear
                                ss << "-W -g 1 1 ";
                                imageInfo.gamma=LIN_G;
                                imageInfo.non_linear_range=LIN_N;
                                break;
                        case 8: //8bit IUT BT.709
                                ss << "-W -g 2.222 4.5 ";
                                imageInfo.gamma=BT709_G;
                                imageInfo.non_linear_range=BT709_N;
                                break;
                        case 9: //8bit sRGB
                                ss << "-W -g 2.4 12.8 ";
                                imageInfo.gamma=SRGB_G;
                                imageInfo.non_linear_range=SRGB_N;
                                break;
                        case 0: //8bit IUT BT.709 (auto WP)
                        default:
                                imageInfo.gamma=BT709_G;
                                imageInfo.non_linear_range=BT709_N;
                                break;
            }
/*
            if (mod("16bit").toi()) {
		ss<<"-4 ";
		imageInfo.gamma=LIN_G;
		imageInfo.non_linear_range=LIN_N;
		}
		else {
		//par défaut dcraw IUT BT.709
		//en fait, non...?
		imageInfo.gamma=SRGB_G;
		imageInfo.non_linear_range=SRGB_N;
		}
*/
            if ( mod("red_mag") != "1.000" || mod("blue_mag") != "1.000" )
                ss << "-C " << mod("red_mag") << " " << mod("blue_mag") << " ";
            ss << "\"" << mod("root") << mod("filename") << "\"" << " > \"" << tfile << "\"";

           int res __attribute__((unused));
           {
                Profile prof("dcraw");
                Logger log("dcraw");log.debug(ss.str());
                res = system((ss.str()).c_str());
           }
           {
                Profile prof("image.read");
                image.read(tfile);
           }
                unlink(tfile.c_str());

           {
                Profile prof("curve creation");
		curve.size("65536x1");
        	Pixels curve_cache(curve);
		PixelPacket *pixels = curve_cache.get(0,0,65536,1);
		if ( mod("discrete").toi() ) {
			double discrete=mod("discrete").tof();
			for(int i=0; i < 65536 ; ++i) {
				unsigned int c = pow(2., 16. - floor( -discrete*log(double(i)/MaxRGB)/log(2) )/discrete );
				if (c > MaxRGB) c=MaxRGB;
				pixels[i].red=pixels[i].green=pixels[i].blue=c;
			}
		}
		else
			for(int i=0; i < 65536 ; ++i)
				pixels[i].red=pixels[i].green=pixels[i].blue=i;
		curve_cache.sync();
		iGamma(curve,imageInfo.gamma,imageInfo.non_linear_range,false);

                int hp_enabled = mod("hp_enabled").toi();
                int hp_aggressive = mod("hp_aggressive").toi();
		if ( hp_enabled ) {
		        if ( imageInfo.iso_speed >= hp_enabled )
			        image = removeHotPixels(image,
			                                pow(2.,mod("hp_delta").tof()),
			                                imageInfo.iso_speed >= hp_aggressive ? 1 : 0,
			                                mod("hp_naive").toi());
		}
	    }
#if 0
            if ( mod("halfSize").toi() == 3 && !imageInfo.filterPattern.empty() ) {
                image.modifyImage();
                int     h = image.rows(),
                        w = image.columns();
                        
                /*
                  RGGB RGGB RGGB RGGB
                  GBRG GBRG GBRG GBRG
                  GRBG GRBG GRBG GRBG
                  BGGR BGGR BGGR BGGR
                  
                  
                  RGRGRG GBGBGB GRGRGR BGBGBG        Rgb RGb Rgb RGb Rgb RGb
                  GBGBGB RGRGRG BGBGBG GRGRGR        RGb RgB RGB RgB RGB RgB
                  RGRGRG GBGBGB GRGRGR BGBGBG        Rgb RGB RgB RGB RgB RGB
                  GBGBGB RGRGRG BGBGBG GRGRGR        RGb RgB RGb RgB RGb RgB
                                                     Rgb rGb Rgb rGb Rgb rGb
                 */

                const char * pattern = imageInfo.filterPattern.c_str();

                Pixels pixel_cache(image);

//#pragma omp parallel for
                for ( int y = 1 ; y < h-1 ; ++y ) {
                        PixelPacket *pixels[3] = { pixel_cache.get(0,y-1,w,1), pixel_cache.get(0,y,w,1), pixel_cache.get(0,y+1,w,1) };
                                       
                        if ( !pixels[0] || !pixels[1] || !pixels[2] ) continue;
                        for ( int x = 1 ; x < w-1 ; ++x ) {

                                switch( pattern[ ((y%4) * 4) + (x%4)  ] ) {
                                        case 'R'://red
                                                pixels[1][x].green = (pixels[0][x].green + pixels[2][x].green + pixels[1][x-1].green + pixels[1][x+1].green)/4;
                                                pixels[1][x].blue = (pixels[0][x-1].blue + pixels[0][x+1].blue + pixels[2][x-1].blue + pixels[2][x+1].blue )/4;
                                                break;
                                        case 'G'://green
                                                if ( pattern[ ((y%4) * 4) + ((x-1)%4)  ] == 'R' ) {
                                                        pixels[1][x].red=(pixels[1][x-1].red+pixels[1][x+1].red)/2;
                                                        pixels[1][x].blue=(pixels[0][x].blue+pixels[2][x].blue)/2;
                                                }
                                                else {
                                                        pixels[1][x].blue=(pixels[1][x-1].blue+pixels[1][x+1].blue)/2;
                                                        pixels[1][x].red=(pixels[0][x].red+pixels[2][x].red)/2;
                                                }

                                                break;
                                        case 'B'://blue
                                                pixels[1][x].green = (pixels[0][x].green + pixels[2][x].green + pixels[1][x-1].green + pixels[1][x+1].green)/4;
                                                pixels[1][x].red = (pixels[0][x-1].red + pixels[0][x+1].red + pixels[2][x-1].red + pixels[2][x+1].red )/4;

                                                break;
                                        case 'C'://cyan
//                                                pixels[x].red=0;
                                                break;
                                        case 'M'://magenta
//                                                pixels[x].green=0;
                                                break;
                                        case 'Y'://yellow
//                                                pixels[x].blue=0;
                                                break;
                                }


                        }
                }
  //      #pragma omp barrier
                pixel_cache.sync();
            }
#endif

        }
    };

    class Negate : public Operator {
        public:
        Negate(int i, ptr<Operator> lop) : Operator("negate",i,lop) {
            mod("grayscale","0");
            serial=0;
        }
        void doJob() {
                Profile prof("Negate");
            image.negate(mod("grayscale").toi());
            curve.negate(mod("grayscale").toi());
        }
    };

    class MicroContrasts : public Operator {
        public:
        MicroContrasts(int i, ptr<Operator> lop) : Operator("microcontrasts",i,lop) {
	  mod("lab","1");
	}
        void doJob() {
	  bool lab=mod("lab").toi()!=0;
            const double kernel[]={0,-1,0,-1,8,-1,0,-1,0};
            Profile prof("MicroContrasts");
	    if (!lab)
	      {
		image.convolve(3,kernel);
	      }
	    else
	      {
		Image labImage=image;
		iGamma(image,imageInfo.gamma,imageInfo.non_linear_range,true); //  g -> linear
                Labize(image,labImage);
		iGamma(labImage,LAB_G,LAB_N,true); // lab -> linear

		labImage.convolve(3,kernel);

		iGamma(labImage,LAB_G,LAB_N,false); // linear -> lab
                unLabize(image,labImage);
		iGamma(image,imageInfo.gamma,imageInfo.non_linear_range,false); // linear -> g



	      }
	    //curve.convolve(3,kernel);
        }
    };

    class BW : public Operator {
        public:
        BW(int i, ptr<Operator> lop) : Operator("bw",i,lop) {
		mod("filter","0");
		serial=0;
	}

	void doFilter(double rf, double gf, double bf) {

	int h = image.rows(),
		w = image.columns();
	image.modifyImage();
	Pixels pixel_cache(image);

	#pragma omp parallel for
		for ( int y = 0 ; y < h ; ++y ) {
			PixelPacket *pixels = pixel_cache.get(0,y,w,1);
			if ( !pixels ) continue;
			for (int x = 0 ; x < w ; ++x ) {
				int c = pixels[x].red * rf
					+ pixels[x].green * gf
					+ pixels[x].blue * bf;
				pixels[x].red=pixels[x].green=pixels[x].blue=c;
			}
		}
	#pragma omp barrier
	pixel_cache.sync();
       	}

        void doJob() {
            Profile prof("BW");

	    if ( mod("filter") != "0" ) {
		    Vector<String> filter = mod("filter").explode(",");
		    if ( filter.size() == 3 )
			    doFilter(filter[0].tof()/100.L,filter[1].tof()/100.L,filter[2].tof()/100.L);
	    }
	    image.type( Magick::GrayscaleType );
        };
    };

    class Flop : public Operator {
        public:
        Flop(int i, ptr<Operator> lop) : Operator("flop",i,lop) {}
        void doJob() {
            Profile prof("flop");
                image.flop();
        }
    };
    class Flip : public Operator {
        public:
        Flip(int i, ptr<Operator> lop) : Operator("flip",i,lop) {}
        void doJob() {
            Profile prof("flip");
            image.flip();
        }
    };
    class RotateCW : public Operator {
        public:
        RotateCW(int i, ptr<Operator> lop) : Operator("rotatecw",i,lop) {}
        void doJob() {
            Profile prof("RotateCW");
            image.rotate(90);
        }
    };
    class RotateCCW : public Operator {
        public:
        RotateCCW(int i, ptr<Operator> lop) : Operator("rotateccw",i,lop) {}
        void doJob() {
            Profile prof("RotateCCW");
            image.rotate(-90);
        }
    };

    class UnsharpMask : public Operator {
        public:
        UnsharpMask(int i, ptr<Operator> lop) : Operator("unsharpmask",i,lop) {
          //  mod("radius","5");
            mod("sigma","1");
            mod("amount","1");
            mod("threshold","0");
            serial=0;
        }
        void doJob() {
            Profile prof("UnsharpMask");
            image.unsharpmask(mod("sigma").tof()*2,mod("sigma").tof(),
                              mod("amount").tof(),mod("threshold").tof());
            /*curve.unsharpmask(mod("sigma").tof()*2,mod("sigma").tof(),
                              mod("amount").tof(),mod("threshold").tof());*/
        }
    };

void Labize(Image &image, Image &labImage) {

		int h = image.rows(),
			w = image.columns();
		image.modifyImage();
		labImage.modifyImage();
		Pixels pixel_cache(image);
		Pixels lab_cache(labImage);

	#pragma omp parallel for
                for (int y = 0 ; y < h ; ++y ) {
			PixelPacket *pixels = pixel_cache.get(0,y,w,1);
			PixelPacket *labpixels = lab_cache.get(0,y,w,1);
			if ( !pixels || !labpixels ) continue;
                    for (int x = 0 ; x < w ; ++x ) {
                        int rgb[3];
                        rgb[0]=pixels[x].red;
                        rgb[1]=pixels[x].green;
                        rgb[2]=pixels[x].blue;
                        int lab[3];
                        RGB_to_CIELab16(rgb,lab);
//                        int l=  int( lab[0] * ( double(MaxRGB)/100.L ) );
//                        l= l<0?0:(l>signed(MaxRGB)?MaxRGB:l);
			labpixels[x].red=
			labpixels[x].green=
			labpixels[x].blue=lab[0];
			}
                }
	#pragma omp barrier
		lab_cache.sync();
		pixel_cache.sync();

}

void unLabize(Image &image, Image &labImage) {
		int h = image.rows(),
			w = image.columns();
		image.modifyImage();
		labImage.modifyImage();
		Pixels pixel_cache(image);
		Pixels lab_cache(labImage);

	#pragma omp parallel for
                for (int y = 0 ; y < h ; ++y ) {
			PixelPacket *pixels = pixel_cache.get(0,y,w,1);
			PixelPacket *labpixels = lab_cache.get(0,y,w,1);
			if ( !pixels || !labpixels ) continue;
                    for (int x = 0 ; x < w ; ++x ) {
                        int rgb[3];
			//triplé rgb de l'image d'origine

                        rgb[0]=pixels[x].red;
                        rgb[1]=pixels[x].green;
                        rgb[2]=pixels[x].blue;
			//conversion du triplé de rgb ver Lab
                        int lab[3];
                        RGB_to_CIELab16(rgb,lab);
                        int l=labpixels[x].red;
//                        lab[0]= l / ( double(MaxRGB)/100.L ) ;

			//copie du L de l'image lab vers le L de l'image RGB
                        lab[0]=l;
                        CIELab16_to_RGB(lab,rgb);
//                        rgb[0]=rgb[0]>signed(MaxRGB)?MaxRGB:rgb[0];
//                        rgb[1]=rgb[1]>signed(MaxRGB)?MaxRGB:rgb[1];
//                        rgb[2]=rgb[2]>signed(MaxRGB)?MaxRGB:rgb[2];
			pixels[x].red=rgb[0];
			pixels[x].green=rgb[1];
			pixels[x].blue=rgb[2];

			}
                }
	#pragma omp barrier
		lab_cache.sync();
		pixel_cache.sync();

}
    class Contrast : public Operator {
        public:
        Contrast(int i, ptr<Operator> lop) : Operator("contrast",i,lop) {
            mod("sharpen","1");
            mod("quantity","1");
            mod("lab","0");
            serial=0;
        }

        void doJob() {
            Profile prof("Contrast");

            int i=mod("quantity").toi();
            bool lab=mod("lab").toi()!=0;
            if (!lab)
                while (i--) {
                    image.contrast(mod("sharpen").toi());
		    curve.contrast(mod("sharpen").toi());
		}
            else {
                //in lab colorspace
                Image labImage=image;

		iGamma(image,imageInfo.gamma,imageInfo.non_linear_range,true); //  g -> linear
                Labize(image,labImage);
		iGamma(labImage,LAB_G,LAB_N,true); // lab -> linear

		iGamma(image,imageInfo.gamma,imageInfo.non_linear_range,false); //  linear -> g

                while (i--)
                    labImage.contrast(mod("sharpen").toi());

		iGamma(image,imageInfo.gamma,imageInfo.non_linear_range,true); //  g -> linear
		iGamma(labImage,LAB_G,LAB_N,false); // linear -> lab

                unLabize(image,labImage);
		iGamma(image,imageInfo.gamma,imageInfo.non_linear_range,false); // linear -> g
            }
        }
    };
    class SigmoidalContrast : public Operator {
        public:
        SigmoidalContrast(int i, ptr<Operator> lop) : Operator("sigmoidalcontrast",i,lop) {
            mod("sharpen","1");
            mod("contrast","1");
            mod("mpdiv","8");
            mod("lab","0");
            serial=0;
        }

        void doJob() {

            Profile prof("SigmoidalContrast");

	bool lab=mod("lab").toi()!=0;

		iGamma(image,imageInfo.gamma,imageInfo.non_linear_range,true); //  g -> linear
		iGamma(curve,imageInfo.gamma,imageInfo.non_linear_range,true); //  g -> linear
            if (!lab) {
                    image.sigmoidalContrast(mod("sharpen").toi(),mod("contrast").tof(),MaxRGB/mod("mpdiv").tof());
                    curve.sigmoidalContrast(mod("sharpen").toi(),mod("contrast").tof(),MaxRGB/mod("mpdiv").tof());
		}
            else {
                //in lab colorspace
                Image labImage=image;
                Labize(image,labImage);
		iGamma(labImage,LAB_G,LAB_N,true); // lab -> linear


                labImage.sigmoidalContrast(mod("sharpen").toi(),mod("contrast").tof(),MaxRGB/mod("mpdiv").tof());

		iGamma(labImage,LAB_G,LAB_N,false); // linear -> lab
                unLabize(image,labImage);

            }
		iGamma(image,imageInfo.gamma,imageInfo.non_linear_range,false); // linear -> g
		iGamma(curve,imageInfo.gamma,imageInfo.non_linear_range,false); // linear -> g

        }
    };

 	class SinusContrast : public Operator {
		public:
		SinusContrast(int i, ptr<Operator> lop) : Operator("sinuscontrast",i,lop) {
			mod("dynamic_range","12");
			mod("linear_extinction","0");
			mod("lab","0");
			mod("channels","7");
			serial=0;
		}
		void doJob() {
		            Profile prof("SinusContrast");

			double dynamic_range=mod("dynamic_range").tof();
		/*
			lx=10**-5
			f(x)=(sin((((x/5)*pi)+pi/2))/2.-1./2.)*(-log((lx)**(1/2.4))/log(10))
			g(x)=10**f(log(x)/log(10))
			plot [x=lx:1] x**(1./2.4),g(x**(16./(12)))

			tanh:
			set logscale xy 2
			f(x)=((tanh(2*x/(log(65536)/log(10))+1)-1-(tanh(1)-1))*log(65536)/log(10)/2)
			g(x)=10**f(log(x)/log(10))
			plot [x=1/(2.**16):1] [1/(2.**8):1] x**(1./2.4),g(x**(16./10))**(1/2.4)

		*/

	//sin douce
	//		#define func_f(x) ( (sin((((x/5.L)*M_PI)+M_PI/2.L))/2.L-1.L/2.L)*(-log(pow(lx,1.L/2.4L))/log(10)) )
	//sin dure
	//		#define func_f(x) ( (sin((((x/5.L)*M_PI))))*(-log(pow     (lx,1.L/2.4L))/log(10)) )
	//tanh - gamma
			#define IL_W 16
			#define IL_H 12
			#define func_f(x) ((tanh(2*x/(log(pow(2,IL_W))/log(10))+1.L)-1.L-(tanh(1)-1.L))*log(pow(2,IL_H))/log(10)/2)
			#define func_g(x) pow(10,func_f(log(x)/log(10)))
			#define func_h(x) func_g(pow(x,16.L/dynamic_range))
			unsigned int lut[MaxRGB+1];
	//		double lx=pow(10,-5);
			bool stop=false;
			unsigned int i = MaxRGB + 1 ;
			do {
				--i;
				double xx=double(i)/double(MaxRGB);
				lut[i]=func_h(xx)*MaxRGB;
				//afin d'éviter que le sin ne remonte
				if ( stop )
					lut[i]=lut[i+1];
				else if ( i!=MaxRGB && lut[i] > lut[i+1] ) {
					stop=true;
					lut[i]=lut[i+1];
				}
			} while ( i > 0 );



			//fait en sorte que le pied soit toujours <=  la droite y=x afin de conserver le point noir
			//valable pour plage dynamique <= 13 car au delà, la courbe est toujours sup
			if ( dynamic_range <= 13.L && mod("linear_extinction").toi() != 0 )
				for (i=0; i<=MaxRGB ; ++i) {
					if ( lut[i] > i ) lut[i]=i;
					else break;
				}

			#undef func_h
			#undef func_g
			#undef func_f
			#undef IL_W
			#undef IL_H

			bool lab=mod("lab").toi();
			int channels = mod("channels").toi();

		unsigned int h = image.rows(),
				w = image.columns();


		iGamma(image,imageInfo.gamma,imageInfo.non_linear_range,true); //  g -> linear
		iGamma(curve,imageInfo.gamma,imageInfo.non_linear_range,true); //  g -> linear

		image.modifyImage();
		Pixels pixel_cache(image);

	#pragma omp parallel for
                for ( int y = 0 ; (unsigned int)y < h ; ++y ) {
			PixelPacket *pixels = pixel_cache.get(0,y,w,1);
			if ( ! pixels ) {
				continue;
			}
                    for (unsigned int x = 0 ; x < w ; ++x ) {

			if ( lab ) {
				int rgb[3]={pixels[x].red,pixels[x].green,pixels[x].blue};
				double lab[3];
				double v;
				RGB_to_CIELab(rgb,lab);
				v= lab_linearize(lab[0]);
				v =  double(lut[int(v*MaxRGB)])/MaxRGB;
				lab[0] = lab_gammaize(v);
				CIELab_to_RGB(lab,rgb);
				pixels[x].red=rgb[0];
				pixels[x].green=rgb[1];
				pixels[x].blue=rgb[2];
			}
			else {
	                        if ( channels & 4 ) pixels[x].red=lut[pixels[x].red];
        	                if ( channels & 2 ) pixels[x].green=lut[pixels[x].green];
        	                if ( channels & 1 ) pixels[x].blue=lut[pixels[x].blue];
			}

                    }
		}
	#pragma omp barrier
			pixel_cache.sync();


		iGamma(image,imageInfo.gamma,imageInfo.non_linear_range,false); //  linear -> g
		curve.modifyImage();
		Pixels curve_cache(curve);
		PixelPacket *pixels = curve_cache.get(0,0,65536,1);
		for ( int i = 0 ; i < 65536 ; ++i ) {
			if ( channels & 4 ) pixels[i].red=lut[pixels[i].red];
			if ( channels & 2 ) pixels[i].green=lut[pixels[i].green];
			if ( channels & 1 ) pixels[i].blue=lut[pixels[i].blue];
		}
		curve_cache.sync();

		iGamma(curve,imageInfo.gamma,imageInfo.non_linear_range,false); //  linear -> g
		/*
			correction gamma pour centrer le milieu de la dynamic au croisement de la courbe de gamma 2.4
			2^(log(16/4)/log(2)+log(1.8)/log(2))
		*/
			//pas bon, blackpoint trop haut
			//image.gamma(pow(2,log(16.L/mod("dynamic_range").tof())/log(2)+log(1.8L)/log(2)));

		}
	};


 	class SoftDR : public Operator {
		public:
		SoftDR(int i, ptr<Operator> lop) : Operator("softdr",i,lop) {
			mod("dynamic_range","12");
			mod("linear_extinction","0");
			mod("lab","0");
			mod("red_diff","0");
//			mod("red_gamma","1");
			mod("blue_diff","0");
//			mod("blue_gamma","1");
                        mod("val","0");
			serial=0;
		}
		void doJob() {
		            Profile prof("SoftDR");

			double dynamic_range=mod("dynamic_range").tof();
			double red_diff=mod("red_diff").tof();
			double blue_diff=mod("blue_diff").tof();
		/*
			lx=10**-5
			f(x)=(sin((((x/5)*pi)+pi/2))/2.-1./2.)*(-log((lx)**(1/2.4))/log(10))
			g(x)=10**f(log(x)/log(10))
			plot [x=lx:1] x**(1./2.4),g(x**(16./(12)))

			tanh:
			set logscale xy 2
			f(x)=((tanh(2*x/(log(65536)/log(10))+1)-1-(tanh(1)-1))*log(65536)/log(10)/2)
			g(x)=10**f(log(x)/log(10))
			plot [x=1/(2.**16):1] [1/(2.**8):1] x**(1./2.4),g(x**(16./10))**(1/2.4)

		*/

	//sin douce
	//		#define func_f(x) ( (sin((((x/5.L)*M_PI)+M_PI/2.L))/2.L-1.L/2.L)*(-log(pow(lx,1.L/2.4L))/log(10)) )
	//sin dure
	//		#define func_f(x) ( (sin((((x/5.L)*M_PI))))*(-log(pow     (lx,1.L/2.4L))/log(10)) )
	//tanh - gamma
			#define IL_W 16
			#define IL_H 12
//			#define func_f(x) ((tanh(2*x/(log(pow(2,IL_W))/log(10))+1.L)-1.L-(tanh(1)-1.L))*log(pow(2,IL_H))/log(10)/2)
			#define func_f(x) ((tanh((val*.30103)+2*x/(log(pow(2,IL_W))/log(10))+1.L)-1.L-(tanh(1+(val*.30103))-1.L))*log(pow(2,IL_H))/log(10)/2)
			#define func_g(x) pow(10,func_f(log(x)/log(10)))
			unsigned int lutr[MaxRGB+1],lutg[MaxRGB+1],lutb[MaxRGB+1];
	//		double lx=pow(10,-5);
	                double val = mod("val").tof()/pow(2.,log(dynamic_range)/log(2.)-3.);
			bool stopr=false,stopg=false,stopb=false;
			unsigned int i = MaxRGB + 1 ;
			do {
				--i;
				double xx=double(i)/double(MaxRGB);
				lutr[i]=func_g(pow(xx,16.L/(dynamic_range+red_diff)))*MaxRGB;
				lutg[i]=func_g(pow(xx,16.L/dynamic_range))*MaxRGB;
				lutb[i]=func_g(pow(xx,16.L/(dynamic_range+blue_diff)))*MaxRGB;

				//afin d'éviter que le sin ne remonte
				if ( stopr )
					lutr[i]=lutr[i+1];
				else if ( i!=MaxRGB && lutr[i] > lutr[i+1] ) {
					stopr=true;
					lutr[i]=lutr[i+1];
				}
				if ( stopg )
					lutg[i]=lutg[i+1];
				else if ( i!=MaxRGB && lutg[i] > lutg[i+1] ) {
					stopg=true;
					lutg[i]=lutg[i+1];
				}
				if ( stopb )
					lutb[i]=lutb[i+1];
				else if ( i!=MaxRGB && lutb[i] > lutb[i+1] ) {
					stopb=true;
					lutb[i]=lutb[i+1];
				}

			} while ( i > 0 );



			//fait en sorte que le pied soit toujours <=  la droite y=x afin de conserver le point noir
			//valable pour plage dynamique <= 13 car au delà, la courbe est toujours sup

			#undef func_g
			#undef func_f
			#undef IL_W
			#undef IL_H

			bool lab=mod("lab").toi();

		unsigned int h = image.rows(),
				w = image.columns();

		Pixels *pixel_cache;
		iGamma(image,imageInfo.gamma,imageInfo.non_linear_range,true); //  g -> linear
		iGamma(curve,imageInfo.gamma,imageInfo.non_linear_range,true); //  g -> linear
		Image labImage=image;
		if ( lab ) {
			Labize(image,labImage);
			iGamma(labImage,LAB_G,LAB_N,true); // lab -> linear
			labImage.modifyImage();
			pixel_cache= new Pixels(labImage);
		}
		else {
			image.modifyImage();
			pixel_cache= new Pixels(image);
		}
#pragma omp parallel for
                for ( int y = 0 ; (unsigned int)y < h ; ++y ) {
			PixelPacket *pixels = pixel_cache->get(0,y,w,1);
			if ( ! pixels ) {
				continue;
				//throw ServletException("pixel packet array is null");
			}
                    for (unsigned int x = 0 ; x < w ; ++x ) {
                        //Magick::Color col = image.pixelColor(x,y);
                        //Magick::Color col = pixels[x];
                        //int rgb[3];
                        pixels[x].red=lutr[pixels[x].red];
                        pixels[x].green=lutg[pixels[x].green];
                        pixels[x].blue=lutb[pixels[x].blue];
//                        image.pixelColor(x,y,Color(rgb[0],rgb[1],rgb[2],0));
                    }
		}
#pragma omp barrier
			pixel_cache->sync();
			delete pixel_cache;

		if ( lab ) {
			iGamma(labImage,LAB_G,LAB_N,false); // linear -> lab
			unLabize(image,labImage);
		}
//		image.gamma(mod("red_gamma").tof(),1.L,mod("blue_gamma").tof());
		image.gamma(pow(dynamic_range/(dynamic_range+red_diff),1.32193L),1.L,
			    pow(dynamic_range/(dynamic_range+blue_diff),1.32193L));
		iGamma(image,imageInfo.gamma,imageInfo.non_linear_range,false); //  linear -> g

		curve.modifyImage();
		pixel_cache = new Pixels(curve);
		PixelPacket *pixels = pixel_cache->get(0,0,65536,1);
		for ( int i = 0 ; i < 65536 ; ++i ) {
			pixels[i].red=lutr[pixels[i].red];
			pixels[i].green=lutg[pixels[i].green];
			pixels[i].blue=lutb[pixels[i].blue];
		}
		pixel_cache->sync();
		delete pixel_cache;
//		curve.gamma(mod("red_gamma").tof(),1.L,mod("blue_gamma").tof());
		curve.gamma(pow(dynamic_range/(dynamic_range+red_diff),1.32193L),1.L,
			    pow(dynamic_range/(dynamic_range+blue_diff),1.32193L));
		iGamma(curve,imageInfo.gamma,imageInfo.non_linear_range,false); //  linear -> g
		/*
			correction gamma pour centrer le milieu de la dynamic au croisement de la courbe de gamma 2.4
			2^(log(16/4)/log(2)+log(1.8)/log(2))
		*/
			//pas bon, blackpoint trop haut
			//image.gamma(pow(2,log(16.L/mod("dynamic_range").tof())/log(2)+log(1.8L)/log(2)));

		}
	};

    class Enhance : public Operator {
        public:
        Enhance(int i, ptr<Operator> lop) : Operator("enhance",i,lop) {}
        void doJob() {
            Profile prof("Enhance");
            image.enhance();
		//curve.enhance();
        }
    };

    class Despeckle : public Operator {
        public:
        Despeckle(int i, ptr<Operator> lop) : Operator("despeckle",i,lop) {}
        void doJob() {
            Profile prof("Despeckle");
            image.despeckle();
	    //curve.despeckle();
        }
    };

    class AdaptiveThreshold : public Operator {
        public:
        AdaptiveThreshold(int i, ptr<Operator> lop) : Operator("adaptive_threshold",i,lop) {
            mod("width","4");
            mod("height","4");
            mod("offset","0");
            serial=0;
        }
        void doJob() {
                    Profile prof("AdaptativeThreshold");
            image.adaptiveThreshold(mod("width").toi(),mod("height").toi(),mod("offset").toi());
            //curve.adaptiveThreshold(mod("width").toi(),mod("height").toi(),mod("offset").toi());
        }
    };

    class ReduceNoise : public Operator {
        public:
        ReduceNoise(int i, ptr<Operator> lop) : Operator("reduce_noise",i,lop) {
            mod("order","1");
            serial=0;
        }
        void doJob() {
            Profile prof("ReduceNoise");
            if ( mod("order").empty() )
                image.reduceNoise();
            else
                image.reduceNoise(mod("order").toi());
        }
    };

    class Segment : public Operator {
        public:
        Segment(int i, ptr<Operator> lop) : Operator("segment",i,lop) {
            mod("cluster","1.0");
            mod("smoothing","1.5");
            serial=0;
        }
        void doJob() {
            Profile prof("Segment");
            image.segment(mod("cluster").tof(),mod("smoothing").tof());
            //curve.segment(mod("cluster").tof(),mod("smoothing").tof());
        }
    };


    class Equalize : public Operator {
        public:
        Equalize(int i, ptr<Operator> lop) : Operator("equalize",i,lop) {}
        void doJob() {
            Profile prof("Equalize");
            image.equalize();
	    //curve.equalize();
        }
    };

    class Normalize : public Operator {
        public:
        Normalize(int i, ptr<Operator> lop) : Operator("normalize",i,lop) {}
        void doJob() {
            Profile prof("Normalize");
            image.normalize();
	    curve.normalize();
        }
    };



    class IGamma : public Operator {

        public:
        IGamma(int i, ptr<Operator> lop) : Operator("igamma",i,lop) {
                mod("gamma","2.4");		//gamma
		mod("dynamic_range","8.36171");	//x0=1/(2^dynamic_range)
		mod("invert","0");
		mod("channels","3");
                serial=0;
        }
        void doJob() {
            Profile prof("IGamma");
            imageInfo.gamma=mod("gamma").tof();
	    imageInfo.non_linear_range=1.L/pow(2.L,mod("dynamic_range").tof());
		iGamma(image,imageInfo.gamma,imageInfo.non_linear_range,mod("invert").toi(),mod("channels").toi());
		iGamma(curve,imageInfo.gamma,imageInfo.non_linear_range,mod("invert").toi(),mod("channels").toi());

        }
    };


    class Gamma : public Operator {

        public:
        Gamma(int i, ptr<Operator> lop) : Operator("gamma",i,lop) {
                mod("gamma","2.2");
                serial=0;
        }
        void doJob() {
            Profile prof("Gamma");
            imageInfo.gamma=mod("gamma").tof();
            imageInfo.non_linear_range=1.L;
            image.gamma(imageInfo.gamma);
            curve.gamma(imageInfo.gamma);
        }
    };

    class RGBGamma : public Operator {

        public:
        RGBGamma(int i, ptr<Operator> lop) : Operator("rgbgamma",i,lop) {
                mod("red","1");
                mod("green","1");
                mod("blue","1");
                serial=0;
        }
        void doJob() {
/*		imageInfo.gamma=mod("gamma").tof();
		imageInfo.non_linear_range=1.L;
*/
            Profile prof("RGBGamma");
            image.gamma(mod("red").tof(),mod("green").tof(),mod("blue").tof());
            curve.gamma(mod("red").tof(),mod("green").tof(),mod("blue").tof());
        }
    };
    class ZoneSystem : public Operator {
    	public:
	ZoneSystem(int i, ptr<Operator> lop) : Operator("zonesystem",i,lop) {
		mod("gamma","2.2");
		serial=0;
	}
	void doJob() {
                Profile prof("ZoneSystem");
		unsigned int x,y;
		//correction du gamma d'entrée

		//iGamma(image,imageInfo.gamma,imageInfo.non_linear_range,true);
		iGamma(image,SRGB_G,SRGB_N,true);

		/*
		//baisse la luminosité de 1/2IL afin de centrer 12.5 au milieu de la zone V
		image.modulate(100.L*M_SQRT1_2l,100,0);

		for ( y = 0 ; y < image.rows() ; ++y )
			for ( x = 0 ; x < image.columns() ; ++x ) {
				Magick::Color col = image.pixelColor(x,y);
				int //r=col.redQuantum(),
				    g=col.greenQuantum();
				    //b=col.blueQuantum();
				        if ( g >= int(double(MaxRGB)*M_SQRT1_2l) ) image.pixelColor(x,y,Color(MaxRGB,MaxRGB,0,0));			//cramé
					else if ( g & (1<<15) ) image.pixelColor(x,y,Color(MaxRGB/1,MaxRGB/1,MaxRGB/1,0));	//VIII	32768	->	65535		50-100%		12b  +3
					else if ( g & (1<<14) ) image.pixelColor(x,y,Color(0*MaxRGB/2,MaxRGB/2,MaxRGB/2,0));	//VII	16384	->	32767		25-50%		11b  +2
					else if ( g & (1<<13) ) image.pixelColor(x,y,Color(0*MaxRGB/4,MaxRGB/4,MaxRGB/4,0));	//VI	8192	->	16383		13-25%		10b  +1
					else if ( g & (1<<12) ) image.pixelColor(x,y,Color(0*MaxRGB/8,MaxRGB/8,0*MaxRGB/8,0));	//V	4096	->	8191		6-13%		9b
					else if ( g & (1<<11) ) image.pixelColor(x,y,Color(MaxRGB/16,MaxRGB/16,MaxRGB/16,0));	//IV	2048	->	4095		3-6%		8b   -1
					else if ( g & (1<<10) ) image.pixelColor(x,y,Color(MaxRGB/32,MaxRGB/32,MaxRGB/32,0));	//III	1024	->	2047		1.5-3%		7b   -2
					else if ( g & (1<<9) ) image.pixelColor(x,y,Color(MaxRGB/64,MaxRGB/64,MaxRGB/64,0));	//II	512	->	1024		0.75-1.5%	6b   -3
					else if ( g & (1<<8) ) image.pixelColor(x,y,Color(MaxRGB/128,MaxRGB/128,MaxRGB/128,0));	//I	256	->	511		0.4-0.75%	5b   -4
//					else if ( g & (1<<7) ) image.pixelColor(x,y,Color(MaxRGB/256,MaxRGB/256,MaxRGB/256,0));	//0
					else image.pixelColor(x,y,Color(MaxRGB,0,0,0));						//bouché                                                     -5

		}
		*/

		int max=0;
		for ( y = 0 ; y < image.rows() ; ++y )
			for ( x = 0 ; x < image.columns() ; ++x ) {
				Magick::Color col = image.pixelColor(x,y);
				int //r=col.redQuantum(),
					//Y de l'espace XYZ
				    g=(0.299*col.redQuantum()+0.587*col.greenQuantum()+0.114*col.blueQuantum());
					if ( g > max ) max=g;
				    //b=col.blueQuantum();
				        if ( (unsigned int)g >= ( MaxRGB - 2 ) ) image.pixelColor(x,y,Color(MaxRGB,MaxRGB,0,0));	//VIII	cramé				+2.5 cramé
					else if ( g & (1<<15) ) image.pixelColor(x,y,Color(MaxRGB/1,MaxRGB/1,MaxRGB/1,0));		//VII	50-100%		12b  +2.5-+1.5	+2
					else if ( g & (1<<14) ) image.pixelColor(x,y,Color(0*MaxRGB/2, MaxRGB/2,  MaxRGB/2,0));		//VI	25-50%		11b  +1.5-+0.5	+1   peau

					else if ( g & (1<<13) ) image.pixelColor(x,y,Color(0*MaxRGB/4, MaxRGB/4,0*MaxRGB/4,0));		//V	12-25%		10b  +0.5--0.5	 0   ciel bleu du nord

					else if ( g & (1<<12) ) image.pixelColor(x,y,Color(  MaxRGB/8, MaxRGB/8,  MaxRGB/8,0));		//IV	6-12%		9b   -0.5--1.5	-1
					else if ( g & (1<<11) ) image.pixelColor(x,y,Color(0*MaxRGB/16,0*MaxRGB/16, 3*MaxRGB/16,0));	//III	3-6%		8b   -1.5--2.5	-2   ombres
					else if ( g & (1<<10) ) image.pixelColor(x,y,Color(  MaxRGB/32,MaxRGB/32, MaxRGB/32,0));	//II	1.5-3%		7b   -2.5--3.5	-3   noirs
					else if ( g & (1<<9) ) image.pixelColor(x,y,Color(  MaxRGB/64,MaxRGB/64, MaxRGB/32,0));		//I	0.75-1.5%	6b   -3.5--4.5	-4
					else if ( g & (1<<8) ) image.pixelColor(x,y,Color(  MaxRGB/128,MaxRGB/128, MaxRGB/128,0));	//0	0.37-0.75%	5b   -4.5--5.5	-5
					else image.pixelColor(x,y,Color(0*MaxRGB,0,0,0));							//neg	0.75-1.5%	4b   -5.5- 	-6   bouché

		}


		// gamma de sortie à 2.2 pour les niveaux de gris
		image.gamma(2.2L);

	}
    };

    class Histogram : public Operator {
        public:
        Histogram(int i, ptr<Operator> lop) : Operator("histogram",i,lop) {
	  mod("log","1");
	  mod("adt","0");
	}
        void doJob() {
            Profile prof("Histogram");
            //image.scale(Magick::Geometry(256,768));
	    bool adt = mod("adt").toi()?true:false;
	    bool hlog = mod("log").toi()?true:false;
        unsigned int x,y;

        int     histo[256][3]={{0}},
                maxi=0;

            for ( y = 0 ; y < image.rows() ; ++y )
                for ( x = 0 ; x < image.columns() ; ++x ) {
                Magick::Color col = image.pixelColor(x,y);
                unsigned int r=col.redQuantum()/257,
                    g=col.greenQuantum()/257,
                    b=col.blueQuantum()/257;
                    ++(histo[r][0]);
                    ++(histo[g][1]);
                    ++(histo[b][2]);
                if ( r > 2 && r < 255 && histo[r][0] > maxi ) maxi=histo[r][0];
                if ( g > 2 && g < 255 && histo[g][1] > maxi ) maxi=histo[g][1];
                if ( b > 2 && b < 255 && histo[b][2] > maxi ) maxi=histo[b][2];
            }
            //image = Image( "256x768" , "black" );
            image = Image( "256x256" , "black" );
            for ( x=0 ; x < 256 ; ++x ) {
	      
/*
                unsigned char qr = 255*pow((double)histo[x][0]/(double)maxi,.5);
                unsigned char qg = 255*pow((double)histo[x][1]/(double)maxi,.5);
                unsigned char qb = 255*pow((double)histo[x][2]/(double)maxi,.5);
*/
	      unsigned char qr;
	      unsigned char qg;
	      unsigned char qb;
	      if ( hlog )
		{
		  qg = 255*log(histo[x][1])/log(maxi);
		  qr = 255*log(histo[x][0])/log(maxi);
		  qb = 255*log(histo[x][2])/log(maxi);
		}
	      else
		{
		  qg = 255*(double)histo[x][1]/(double)maxi;
		  qr = 255*(double)histo[x][0]/(double)maxi;
		  qb = 255*(double)histo[x][2]/(double)maxi;
		}

	if ( qr > 255 ) qr = 255;
	if ( qg > 255 ) qg = 255;
	if ( qb > 255 ) qb = 255;

	if ( qr == 0 && histo[x][0] > 0 ) qr=1;
	if ( qg == 0 && histo[x][1] > 0 ) qg=1;
	if ( qb == 0 && histo[x][2] > 0 ) qb=1;
#if 0
                for (int i = 0 ; i < qr ; ++i )
                        image.pixelColor(x,255-i+0*256,Color(MaxRGB,0,0,0));
                for (int i = 0 ; i < qg ; ++i )
                        image.pixelColor(x,255-i+1*256,Color(0,MaxRGB,0,0));
                for (int i = 0 ; i < qb ; ++i )
                        image.pixelColor(x,255-i+2*256,Color(0,0,MaxRGB,0));
		if ( x == 188 || x == 137 || x == 99 || x == 71
		     || x == 49 || x == 34 || x == 22 || x == 13 || x == 6 )
			for (int i = 0 ; i < 256*3 ; ++ i )
				image.pixelColor(x,i,Color(MaxRGB,MaxRGB,MaxRGB,0));
#else
                for (int i = 0 ; i < qr ; ++i )
		  {
		    Magick::Color col = image.pixelColor(x,255-i);    
		    image.pixelColor(x,255-i,Color(MaxRGB,col.greenQuantum(),col.blueQuantum(),0));
		  }
                for (int i = 0 ; i < qg ; ++i )
		  {
		    Magick::Color col = image.pixelColor(x,255-i);    
		    image.pixelColor(x,255-i,Color(col.redQuantum(),MaxRGB,col.blueQuantum(),0));
		  }
                for (int i = 0 ; i < qb ; ++i )
		  {
		    Magick::Color col = image.pixelColor(x,255-i);    
		    image.pixelColor(x,255-i,Color(col.redQuantum(),col.greenQuantum(),MaxRGB,0));
		  }
		if ( x == 188 || x == 137 || x == 99 || x == 71
		     || x == 49 || x == 34 || x == 22 || x == 13 || x == 6 )
		        //for (int i = 0 ; i < 256*3 ; ++ i )
			for (int i = 0 ; i < 256 ; ++ i )
				image.pixelColor(x,i,Color(MaxRGB,MaxRGB,MaxRGB,0));
#endif
            }
	    if ( adt )
	      image.adaptiveThreshold(4,4,0);
        }
    };

	class LCA : public Operator {
		public:
		LCA(int i, ptr<Operator> lop) : Operator("lca",i,lop) {}
		void doJob() {
                        Profile prof("LCA");
			Vector<int> r,g,b;
			int w = image.columns(),
				h = image.rows();
			r.push_back(0);g.push_back(0);b.push_back(0);
			for ( int x = 1 ; x < w ; ++x ) {
				Magick::Color c = image.pixelColor(x,h/2),
						lc = image.pixelColor(x-1,h/2);


				r.push_back(r[x-1]+(abs(c.redQuantum()-lc.redQuantum()) > 64 ? c.redQuantum()-lc.redQuantum() : 0)   );
				g.push_back(g[x-1]+(abs(c.greenQuantum()-lc.greenQuantum()) > 64 ? c.greenQuantum()-lc.greenQuantum() :0) );
				b.push_back(b[x-1]+(abs(c.blueQuantum()-lc.blueQuantum()) > 64 ? c.blueQuantum()-lc.blueQuantum() : 0 ) );

/*
				r.push_back( c.redQuantum() );
				g.push_back( c.greenQuantum() );
				b.push_back( c.blueQuantum() );
*/
			}

			image = Image ( (itostring(w)+"x"+itostring(h)).c_str() , "black" );

			for ( int x = 0 ; x < w-1 ; ++x ) {
				int y = h/2+((r[x]))/256/*-h/4*/;
				if ( y >= h ) y=h-1;if ( y < 0 ) y=0;

				image.pixelColor(x,y,Color(MaxRGB,0,0,0));

				y = h/2+((g[x]))/256;
				if ( y >= h ) y=h-1;if ( y < 0 ) y=0;
				Magick::Color c = image.pixelColor(x,y);
				image.pixelColor(x,y,Color(c.redQuantum(),MaxRGB,c.blueQuantum(),0));

				y = h/2+((b[x]))/256/*+h/4*/;
				if ( y >= h ) y=h-1;if ( y < 0 ) y=0;
				c = image.pixelColor(x,y);
				image.pixelColor(x,y,Color(c.redQuantum(),c.greenQuantum(),MaxRGB,0));
			}
		}

	};

	class HotPixels : public Operator {
	public:
		HotPixels(int i, ptr<Operator> lop) : Operator("hotpixels",i,lop) {
			mod("delta","0.5");
			mod("aggressive","0");
			mod("naive","0");
			serial=0;
		}
		void doJob() {
                        Profile prof("HotPixels");
			double delta = pow(2.,mod("delta").tof());
			bool aggressive = mod("aggressive").toi();
			bool naive = mod("naive").toi();

			image = removeHotPixels(image,delta,aggressive,naive);

		}
	};


	class ShowCurve: public Operator {
	public:
	ShowCurve(int i, ptr<Operator> lop) : Operator("showcurve",i,lop) {
		mod("mode","1");
		mod("zoneV_18","0");
		serial=0;
	}


	double calcInvGamma(double v) {
		double a=-(SRGB_G-1.L)*pow(SRGB_N,(1.L/SRGB_G))/((SRGB_G-1.L)*pow(SRGB_N,(1.L/SRGB_G))-SRGB_G);
		double x0=(1.L+a)*pow(SRGB_N,1.L/SRGB_G)-a;
		a= (SRGB_G-1.L)*x0;
		double p = SRGB_G*pow((x0+a)/(a+1.L),SRGB_G)/(x0+a);
		if ( v < x0 ) return v/p;
		else return pow((v+a)/(a+1.L),SRGB_G);
	}
	double calcGamma(double v) {
		double a=-(SRGB_G-1.L)*pow(SRGB_N,(1.L/SRGB_G))/((SRGB_G-1.L)*pow(SRGB_N,(1.L/SRGB_G))-SRGB_G);
		double p = (a+1.L)*pow(SRGB_N,1.L/SRGB_G)/(SRGB_G*SRGB_N);
		if ( v < SRGB_N ) return v*p;
		else return (1.L+a)*pow(v,1.L/SRGB_G)-a;

	}

	void doJob() {
                Profile prof("ShowCurve");
		image = Image("512x512","black");
		bool zoneV_18=mod("zoneV_18").toi();

		int mode=mod("mode").toi();
		// mode 0 -> niveaux sRGB, 1 -> lineaire, 2 -> log2

		//lignes verticales (entrée)
		for ( int x = 32 ; x < 512 ; x+=32 ) {
			int c=MaxRGB/4;
			if ( x == 512 - 3*32 ) c*=2;
		//	if ( x == 512 - 9*32 ) c*=2;
			if ( x == 512 - 12*32 ) c*=1.5;
			for ( int y = 0 ; y < 512 ; y++ ) {
				image.pixelColor(x+(zoneV_18?16:0),y,Color(c,c,c,0));
			}
		}

		//lignes horizontales (sortie)
		for ( int il = 1; il < 16 ; ++il ) {
			int c=MaxRGB/4;
			if ( il == 3 ) c*=2;
			if ( il == 9 ) c*=2;   //limite pratique
			if ( il == 12 ) c*=1.5; //limite theorique
			//double v=1.L/pow(2.L,16-il);
			int y=128;
			switch(mode) {
				case 0: //niveaux sRGB
				{
					double v=calcGamma(1.L/pow(2.L,il));
					y=v*512.L-1.L;
				}
				break;
				case 1: //lineaire
				case 2: //log
					y = (16.L-il)*32.L-1.L;
				break;
			}
			if ( y < 0 ) y = 0;
			else if ( y > 511 ) y = 511;
			for ( int x = 0 ; x < 512 ; ++x ) {
				image.pixelColor(x,y,Color(c,c,c,0));
			}
		}


		//courbe

		if ( mode == 1 )
			iGamma(curve,SRGB_G,SRGB_N,true);
		else if ( mode == 2 )
			curve.gamma(1.L/2.2L);

		for ( int x=0 ; x < 65536 ; ++x ) {
		Magick::Color col = curve.pixelColor(x,0);
			double x0 = log(double(x+1)/65536.L)/log(2);
			// x0 compris entre 0 et -16
			int i = (16.L+x0)*32.L-1.L;
			int yr=0,yg=0,yb=0;
			switch ( mode ) {
				case 0: // niveaux sRGB
					yr=col.redQuantum()/128.L;
					yg=col.greenQuantum()/128.L;
					yb=col.blueQuantum()/128.L;
					break;
				case 1: // lineaire
				case 2: { //log-2
					double  v = double(col.redQuantum()+1)/double(MaxRGB+1);
						v = log(v)/log(2);
					yr=  (16.L+v)*32.L-1.L;
					if ( yr < 0 ) yr = 0;
					else if ( yr > 511 ) yr = 511;
						v = double(col.greenQuantum()+1)/double(MaxRGB+1);
						v = log(v)/log(2);
					yg=  (16.L+v)*32.L-1.L;
					if ( yg < 0 ) yg = 0;
					else if ( yg > 511 ) yg = 511;
						v = double(col.blueQuantum()+1)/double(MaxRGB+1);
						v = log(v)/log(2);
					yb=  (16.L+v)*32.L-1.L;
					if ( yb < 0 ) yb = 0;
					else if ( yb > 511 ) yb = 511;
					break;
				}
				default://plop
					i=256;
			}
			if ( i < 0 ) i = 0;
			else if ( i > 511 ) i = 511;
			Magick::Color colr = image.pixelColor(i,yr);
			image.pixelColor(i,yr,
				Color(MaxRGB,colr.greenQuantum(),colr.blueQuantum(),0));
			Magick::Color colg = image.pixelColor(i,yg);
			image.pixelColor(i,yg,
				Color(colg.redQuantum(),MaxRGB,colg.blueQuantum(),0));
			Magick::Color colb = image.pixelColor(i,yb);
			image.pixelColor(i,yb,
				Color(colb.redQuantum(),colb.greenQuantum(),MaxRGB,0));
		}

/*
		for ( int x=0 ; x < 512 ; ++x) {
		Magick::Color col = curve.pixelColor(x*128,0);
		int c = col.redQuantum();
			image.pixelColor(x,
				c/128,
			Color(MaxRGB,MaxRGB,MaxRGB,0));
		}
*/
		image.flip();
		}
	};

    class Linearize: public Operator {
        static int logize(int bias, double mul, int i) {
	int ret =  int(log(double(i)/double(bias))/log(2.L)*mul)+1;
            if ( ret > signed(MaxRGB)) return MaxRGB;
            if ( ret < 0 ) return 0;
            return ret;
        }
        public:
        Linearize(int i, ptr<Operator> lop) : Operator("linearize",i,lop) {
            mod("dynamic_range","6");
            mod("restore_gp","1");
            serial=0;
        }
        void doJob() {
                        Profile prof("Linearize");
/*
#gnuplot
plot [t=0:65535] [0:65535] \
((log(t/127)/log(2)*7272.65197/65535))**(1/0.584058763390)*65536 , \
((log(t/255)/log(2)*8186.2445/65535))**(1/0.677463872646)*65536 , \
((log(t/511)/log(2)*9358.544295/65535))**(1/0.806922582273)*65536 , \
((log(t/1023)/log(2)*10920.14/65535))**(1/1)*65536, \
((log(t/2047)/log(2)*13105.41/65535))**(1/1.321632730781)*65536 , \
((log(t/4095)/log(2)*16382.647/65535))**(1/1.999642736726)*65536 , \
((log(t/8191)/log(2)*21844.21/65535))**(1/14.248816246855)*65536 , \
((log(t/16383)/log(2)*32766.91797/65535)*65536) , \
((log(t/32767)/log(2)*65534.5573/65535)*65536) , \
(t/65536)**(1/2.2)*65536 smooth csplines;

#wcalc
il = 9
MaxRGB = 65535
bias = ((MaxRGB+1)/(2**il))-1
mul = MaxRGB/(ln(MaxRGB/bias)/ln(2))
i= MaxRGB/8
gamma = ln(((ln(i/bias)/ln(2))*mul)/MaxRGB)/ln(0.5)

            */
		imageInfo.gamma=SRGB_G;
		imageInfo.non_linear_range=SRGB_N;
            int x,y;
            double il=mod("dynamic_range").tof();
            double bias=((MaxRGB+1)/(pow(2.L,il)))-1;
            double mul = double(MaxRGB)/(log(double(MaxRGB)/bias)/log(2.L));

		int h = image.rows(),
			w = image.columns();

		image.modifyImage();
		Pixels pixel_cache(image);

//	#pragma omp parallel for
            for ( y = 0 ; y < h ; ++y ) {
			PixelPacket *pixels = pixel_cache.get(0,y,w,1);
			if ( !pixels ) continue;
                for ( x = 0 ; x < w ; ++x ) {
		pixels[x].red=logize(bias,mul,pixels[x].red);
		pixels[x].green=logize(bias,mul,pixels[x].green);
		pixels[x].blue=logize(bias,mul,pixels[x].blue);


            }
	}
//	#pragma omp barrier
	pixel_cache.sync();

	curve.modifyImage();
	Pixels curve_cache(curve);
	PixelPacket *pixels = curve_cache.get(0,0,65536,1);
	for ( x = 0 ; x < 65536 ; ++x ) {
               pixels[x].red=logize(bias,mul,pixels[x].red);
               pixels[x].green=logize(bias,mul,pixels[x].green);
               pixels[x].blue=logize(bias,mul,pixels[x].blue);

	}
	curve_cache.sync();

		if ( mod("restore_gp").toi() == 1 ) {
                    image.level(0,MaxRGB,log(double(logize(bias,mul,MaxRGB/8))/double(MaxRGB))/log(0.5));
                    curve.level(0,MaxRGB,log(double(logize(bias,mul,MaxRGB/8))/double(MaxRGB))/log(0.5));
		}
        }
    };

    class DynamicRange : public Operator {
    	public:
	DynamicRange(int i, ptr<Operator> lop) : Operator("dynamic_range",i,lop) {
		mod("value","100");
		mod("dynamic_range","6");
		mod("restore_gp","1");
		serial=0;
	}
	void doJob() {

              Profile prof("DynamicRange");

		image.modulate(mod("value").tof(),100,100);
		double blackpoint=pow(2,16.L-mod("dynamic_range").tof())-1;
		double g12_5=MaxRGB/8;
		double ng12_5=(g12_5-blackpoint)*double(MaxRGB)/double(double(MaxRGB)-blackpoint);

		if ( mod("restore_gp").toi() ) {
			image.level(blackpoint,MaxRGB,log(double(ng12_5)/double(MaxRGB))/log(double(g12_5)/double(MaxRGB)));
			curve.level(blackpoint,MaxRGB,log(double(ng12_5)/double(MaxRGB))/log(double(g12_5)/double(MaxRGB)));
		}
		else {
			image.level(blackpoint,MaxRGB,1.L);
			curve.level(blackpoint,MaxRGB,1.L);
		}

	}
    };

    class Level : public Operator {
    	public:
	Level(int i, ptr<Operator> lop) : Operator("level",i,lop) {
		mod("blackpoint","0.5");
		mod("whitepoint","99.5");
		mod("gamma","1");
	}
	void doJob() {
            Profile prof("Level");

		int x,y;
		volatile int     histo[256][3]={{0}};
		int total=0,i;

		int h = image.rows(),
			w = image.columns();
		image.modifyImage();
		Pixels pixel_cache(image);
	#pragma omp parallel for
		for ( y = 0 ; y < h ; ++y ) {
			PixelPacket *pixels = pixel_cache.get(0,y,w,1);
			if ( !pixels ) continue;
			for ( x = 0 ; x < w ; ++x ) {
				int r=pixels[x].red/257,
				g=pixels[x].green/257,
				b=pixels[x].blue/257;
				++(histo[r][0]);
				++(histo[g][1]);
				++(histo[b][2]);
			}
		}
	#pragma omp barrier
		pixel_cache.sync();

		double whitepoint=0,blackpoint=0;
		double perc_wp=3*image.rows()*image.columns()*((100.-mod("whitepoint").tof())/100.);
		double perc_bp=3*image.rows()*image.columns()*(mod("blackpoint").tof()/100.);

		total=0;
		//white point
		for ( i=255 ; i>=0 ; --i ) {
			total+=histo[i][0]+histo[i][1]+histo[i][2];
			if ( total >= perc_wp ) {
				whitepoint=i*257.;
				break;
			}
		}

		total=0;
		//black point
		for (i=0 ; i < 256 ; ++i ) {
			total+=histo[i][0]+histo[i][1]+histo[i][2];
			if ( total >= perc_bp ) {
				blackpoint=i*257.;
				break;
			}
		}

		image.level(blackpoint,whitepoint,mod("gamma").tof());
		curve.level(blackpoint,whitepoint,mod("gamma").tof());
	}
    };
    class Lines : public Operator {
        public:
        Lines(int i, ptr<Operator> lop) : Operator("lines",i,lop) {}
        void doJob() {
            Profile prof("Lines");
                unsigned int x,y,or1,or2,tiers1,tiers2;

                double orn=1.61803398874989484820;

                or1=( double(image.columns()) / orn );
                or2=image.columns()-or1;
                tiers1=( double(image.columns()) / 3. );
                tiers2=image.columns()-tiers1;
                for ( y = 0 ; y < image.rows() ; ++y ) {
                        image.pixelColor(or1,y,Color(MaxRGB,MaxRGB,0,0));
                        image.pixelColor(or2,y,Color(MaxRGB,MaxRGB,0,0));
                        image.pixelColor(tiers1,y,Color(MaxRGB,0,0,0));
                        image.pixelColor(tiers2,y,Color(MaxRGB,0,0,0));
                        image.pixelColor(image.columns()/4,y,Color(0,0,0,0));
                        image.pixelColor(image.columns()/2,y,Color(0,0,0,0));
                        image.pixelColor(image.columns()-image.columns()/4,y,Color(0,0,0,0));
                }

                or1=( double(image.rows()) / orn );
                or2=image.rows()-or1;
                tiers1=( double(image.rows()) / 3. );
                tiers2=image.rows()-tiers1;
                for ( x = 0 ; x < image.columns() ; ++x ) {
                        image.pixelColor(x,or1,Color(MaxRGB,MaxRGB,0,0));
                        image.pixelColor(x,or2,Color(MaxRGB,MaxRGB,0,0));
                        image.pixelColor(x,tiers1,Color(MaxRGB,0,0,0));
                        image.pixelColor(x,tiers2,Color(MaxRGB,0,0,0));
                        image.pixelColor(x,image.rows()/4,Color(0,0,0,0));
                        image.pixelColor(x,image.rows()/2,Color(0,0,0,0));
                        image.pixelColor(x,image.rows()-image.rows()/4,Color(0,0,0,0));
                }
        }
    };

    class ColorFilter : public Operator {
        public:
        ColorFilter(int i, ptr<Operator> lop) : Operator("colorfilter",i,lop) {
                mod("redmul","1");
                mod("greenmul","1");
                mod("bluemul","1");
                serial=0;
        }
        template<typename t> t clamp(t v,t min, t max) {
                if ( v < min )
                        return min;
                else if ( v > max )
                        return max;
                else
                        return v;
        }

	void doColorFilter(Image& image, double rm, double gm, double bm) {
		int h = image.rows(),
			w = image.columns();
		image.modifyImage();
		Pixels pixel_cache(image);

	#pragma omp parallel for
                for (int y = 0 ; y < h ; ++y ) {
			PixelPacket *pixels = pixel_cache.get(0,y,w,1);
			if ( !pixels ) continue;
			for (int x = 0 ; x < w ; ++x ) {
                            pixels[x].red=clamp<double>(pixels[x].red*rm,0,MaxRGB);
                            pixels[x].green=clamp<double>(pixels[x].green*gm,0,MaxRGB);
                            pixels[x].blue=clamp<double>(pixels[x].blue*bm,0,MaxRGB);
                 }
		}
	#pragma omp barrier
	pixel_cache.sync();
        }

        void doJob() {
            Profile prof("ColorFilter");
                double  rm=mod("redmul").tof(),
                        gm=mod("greenmul").tof(),
                        bm=mod("bluemul").tof();
		doColorFilter(image,rm,gm,bm);
		doColorFilter(curve,rm,gm,bm);
	}
    };


    class WhiteBalance : public Operator {
        public:
        WhiteBalance(int i, ptr<Operator> lop) : Operator("whitebalance",i,lop) {
                mod("temperature","6500");
                mod("tint","1.0");
                mod("safe","0");
                serial=0;
        }

        //thanks to ufraw_routines.c from ufraw-0.11

        void Temperature_to_RGB(double T, double RGB[3]) {


                const double XYZ_to_RGB[3][3] = {
                                { 3.24071,  -0.969258,  0.0556352 },
                                {-1.53726,  1.87599,    -0.203996 },
                                {-0.498571, 0.0415557,  1.05707 } };

                int c;
                double xD, yD, X, Y, Z, max;
                // Fit for CIE Daylight illuminant
                if (T<= 4000) {
                        xD = 0.27475e9/(T*T*T) - 0.98598e6/(T*T) + 1.17444e3/T + 0.145986;
                } else if (T<= 7000) {
                        xD = -4.6070e9/(T*T*T) + 2.9678e6/(T*T) + 0.09911e3/T + 0.244063;
                } else {
                        xD = -2.0064e9/(T*T*T) + 1.9018e6/(T*T) + 0.24748e3/T + 0.237040;
                }
                yD = -3*xD*xD + 2.87*xD - 0.275;

                // Fit for Blackbody using CIE standard observer function at 2 degrees
                //xD = -1.8596e9/(T*T*T) + 1.37686e6/(T*T) + 0.360496e3/T + 0.232632;
                //yD = -2.6046*xD*xD + 2.6106*xD - 0.239156;

                // Fit for Blackbody using CIE standard observer function at 10 degrees
                //xD = -1.98883e9/(T*T*T) + 1.45155e6/(T*T) + 0.364774e3/T + 0.231136;
                //yD = -2.35563*xD*xD + 2.39688*xD - 0.196035;

                X = xD/yD;
                Y = 1;
                Z = (1-xD-yD)/yD;
                max = 0;
                for (c=0; c<3; c++) {
                        RGB[c] = X*XYZ_to_RGB[0][c] + Y*XYZ_to_RGB[1][c] + Z*XYZ_to_RGB[2][c];
                        if (RGB[c]>max) max = RGB[c];
                }
                for (c=0; c<3; c++) RGB[c] = RGB[c]/max;
        }
        template<typename t> t clamp(t v,t min, t max) {
                if ( v < min )
                        return min;
                else if ( v > max )
                        return max;
                else
                        return v;
        }
        void doJob() {
            Profile prof("WhiteBalance");

                double temperature = clamp<double>(mod("temperature").tof(),2000,12000);
                mod("temperature",ftostring(temperature));

                double RGB[3];
                Temperature_to_RGB(temperature,RGB);

                RGB[1] = RGB[1] / mod("tint").tof();

                RGB[0]=1/RGB[0];
                RGB[1]=1/RGB[1];
                RGB[2]=1/RGB[2];

                double minmul=RGB[0];
                if (minmul > RGB[1] ) minmul=RGB[1];
                if (minmul > RGB[2] ) minmul=RGB[2];
                RGB[0]/=minmul;
                RGB[1]/=minmul;
                RGB[2]/=minmul;

                if ( mod("safe").toi() ) {
                        double maxmul=1;
                        if ( maxmul < RGB[0] ) maxmul=RGB[0];
                        if ( maxmul < RGB[1] ) maxmul=RGB[1];
                        if ( maxmul < RGB[2] ) maxmul=RGB[2];
                        RGB[0]/=maxmul;
                        RGB[1]/=maxmul;
                        RGB[2]/=maxmul;
                }
                //Logger log("W/B");
                //log(String("R=")+ftostring(RGB[0]));
                //log(String("G=")+ftostring(RGB[1]));
                //log(String("B=")+ftostring(RGB[2]));

		int h = image.rows(),
			w = image.columns();
		image.modifyImage();
		Pixels pixel_cache(image);

	#pragma omp parallel for
                for (int y = 0 ; y < h ; ++y ) {
			PixelPacket *pixels = pixel_cache.get(0,y,w,1);
			if ( !pixels ) continue;
			for (int x = 0 ; x < w ; ++x ) {
                                 pixels[x].red=clamp<double>(pixels[x].red*RGB[0],0,MaxRGB);
                                 pixels[x].green=clamp<double>(pixels[x].green*RGB[1],0,MaxRGB);
                                 pixels[x].blue=clamp<double>(pixels[x].blue*RGB[2],0,MaxRGB);
                 	}
		}
	#pragma omp barrier
		pixel_cache.sync();
        }
    };


    class Charcoal : public Operator {
        public:
        Charcoal(int i, ptr<Operator> lop) : Operator("charcoal",i,lop) {
                mod("radius","1");
                mod("sigma","0.5");
                serial=0;
        }
        void doJob() {
            Profile prof("Charcoal");
                image.charcoal(mod("radius").tof(),mod("sigma").tof());
        }
    };

    class Blur : public Operator {
        public:
        Blur(int i, ptr<Operator> lop) : Operator("blur",i,lop) {
                mod("radius","1");
                mod("sigma","0.5");
                serial=0;
        }
        void doJob() {
            Profile prof("Blur");
                image.blur(mod("radius").tof(),mod("sigma").tof());
        }
    };

    class GaussianBlur : public Operator {
        public:
        GaussianBlur(int i, ptr<Operator> lop) : Operator("gaussianblur",i,lop) {
                mod("radius","1");
                mod("sigma","0.5");
                serial=0;
        }
        void doJob() {
            Profile prof("GaussianBlur");
                image.gaussianBlur(mod("radius").tof(),mod("sigma").tof());
        }
    };

    class Emboss : public Operator {
        public:
        Emboss(int i, ptr<Operator> lop) : Operator("emboss",i,lop) {
                mod("radius","1");
                mod("sigma","0.5");
                serial=0;
        }
        void doJob() {
            Profile prof("Emboss");
                image.emboss(mod("radius").tof(),mod("sigma").tof());
        }
    };

    class Implode : public Operator {
        public:
        Implode(int i, ptr<Operator> lop) : Operator("implode",i,lop) {
                mod("factor","1");
                serial=0;
        }
        void doJob() {
            Profile prof("Implode");
                image.implode(mod("factor").tof());
        }
    };


    class SelectiveLabOp : public Operator {
        public:
        SelectiveLabOp(int i, ptr<Operator> lop) : Operator("selectivelabop",i,lop) {
                mod("angle","90");
                mod("saturation","100");
                mod("value","100");
                mod("width","45");
                mod("bias_sat","1");
                mod("bias_val","1");
                mod("inv_sat","0");
                mod("inv_val","0");
                mod("help","0");
                serial = 0;
        }
        
        void selectiveLabOp(Image& image) {
                int     h = image.rows(),
                        w = image.columns();
                
                /*
                static double lut_sin[0x10000];
                static bool lut_ok=false;
                if ( !lut_ok ) {
                        lut_ok=true;
                        // 0 -> -pi 65535 -> +pi
        		#pragma omp parallel for
                        for ( int x = 0 ; x < 0x10000 ; ++x )
                                lut_sin[x]=sin(2.*M_PI*double(x)/double(0xffff) - M_PI);
                }
                */

                image.modifyImage();
                Pixels pixel_cache(image);
                
                //calcul de la puissance de largeur
                double theta = M_PI - M_PI * (mod("width").tof()/2) / 180.;
                double puissance = (log(-M_LN2/(log(-(cos(theta)-1.)/2.)))+2.*M_LN2)/(M_LN2);
                puissance = pow(2.,puissance);
                
                //calcul de l'angle d'application
                theta = M_PI * mod("angle").tof()/180.;
                

                double saturation = mod("saturation").tof()/100.;
                double value = mod("value").tof()/100.;
                double bias_sat = mod("bias_sat").tof();
                double bias_val = mod("bias_val").tof();
                bool inv_sat = mod("inv_sat").toi();
                bool inv_val = mod("inv_val").toi();
                
                /*
                double lut_mul[0x10000];
		#pragma omp parallel for
                for ( int x = 0 ; x < 0x10000; ++x ) {
                        lut_mul[x] = pow((1.-cos( theta +  2.*M_PI*double(x)/double(0xffff) - M_PI ))/2.,puissance);
                }
                */
		#pragma omp parallel for
        	        for ( int y = 0 ; y < h ; ++y ) {
        	                PixelPacket *pixels = pixel_cache.get(0,y,w,1);
        	                if ( !pixels ) continue;
        	                //190ko pour 4000 points de large
        	                for ( int x = 0 ; x < w ; ++x ) {

					int rgb[3]={pixels[x].red,pixels[x].green,pixels[x].blue};
					double lab[3];

					RGB_to_LinearLab(rgb,lab);

                	                double module = sqrt(lab[1]*lab[1]+lab[2]*lab[2]);
                	                
                	                double arg;//= lab[2]/fabs(lab[2])*M_PI/2-atan(-lab[1]/lab[2]);
                	                if ( lab[2] > 0 )
                        	                arg = M_PI/2-atan(-lab[1]/lab[2]);
                	                else
                        	                arg = -M_PI/2-atan(-lab[1]/lab[2]);
                	                double mul= pow((1.-cos(arg+theta))/2.,puissance);
                                        /*
                	                double targ;
                	                if ( lab[2] > 0 )
                        	                targ = M_PI/2-atan(-lab[1]/lab[2]);
                	                else
                        	                targ = -M_PI/2-atan(-lab[1]/lab[2]);
                                        int arg = (1.+targ /M_PI)*0xffff;
                                        */
                	                


                                        // ne pas augmenter la luminosité des parties non saturées d'où le *module/80.8284
                                        // (il semble que 100 soit la valeur max du module)
                                        //double mul_val= inv_val?1.-lut_mul[arg% 0x10000]:lut_mul[arg% 0x10000];
                                        double mul_val= inv_val?1.-mul:mul;
                                        lab[0]*=(bias_val+(value-bias_val)*mul_val*module/80.8284);

                                        //double mul_sat = inv_sat?1.-lut_mul[arg% 0x10000]:lut_mul[arg% 0x10000];
                                        double mul_sat = inv_sat?1.-mul:mul;
                                        module*=(bias_sat+(saturation-bias_sat)*mul_sat);

					//lab[0] = lab_gammaize(v);
					lab[1]=-module * cos(arg);
					lab[2]=module * sin(arg);
                                        //lab[1]=-module * lut_sin[ ( arg + 0x10000/4 ) % 0x10000 ];
                                        //lab[2]=module * lut_sin[ arg % 0x10000];
					LinearLab_to_RGB(lab,rgb);
					pixels[x].red=rgb[0];
					pixels[x].green=rgb[1];
					pixels[x].blue=rgb[2];
        	                }
                	}
        	#pragma omp barrier
                pixel_cache.sync();
        }
        
        void doJob() {

                Profile prof("selectiveLabOp");
		iGamma(image,imageInfo.gamma,imageInfo.non_linear_range,true); // g -> linear
		iGamma(curve,imageInfo.gamma,imageInfo.non_linear_range,true); // g -> linear
		
		if ( mod("help").toi()  ) {
		        image=Image("512x512","black");
                        int     h = image.rows(),
                                w = image.columns();
                        double v = pow(2,mod("help").tof())/(MaxRGB+1);

                        image.modifyImage();
                        Pixels pixel_cache(image);
		#pragma omp parallel for
        	        for ( int y = 0 ; y < h ; ++y ) {
        	                PixelPacket *pixels = pixel_cache.get(0,y,w,1);
        	                if ( !pixels ) continue;
        	                for ( int x = 0 ; x < w ; ++x ) {
					int rgb[3];
					double lab[3];

                                        lab[0] = lab_gammaize(v);
                                        lab[1] = double(x-256)/2.56;
                                        lab[2] = -double(y-256)/2.56;
					CIELab_to_RGB(lab,rgb);
					if ( rgb[0] == MaxRGB || rgb[1] == MaxRGB || rgb[2] == MaxRGB
					|| rgb[0] == 0 || rgb[1] == 0 || rgb[2] == 0 ) {
						pixels[x].red=pixels[x].green=pixels[x].blue=0;
					}
					else {
						pixels[x].red=rgb[0];
						pixels[x].green=rgb[1];
						pixels[x].blue=rgb[2];
					}
        	                }
                	}
        	#pragma omp barrier
                        pixel_cache.sync();
		/*
			unsigned int rgb[3] = {0,0,0};
			for ( rgb[0] = 0 ; rgb[0] < MaxRGB ; rgb[0]+=512 )
			for ( rgb[1] = 0 ; rgb[1] < MaxRGB ; rgb[1]+=512 )
			for ( rgb[2] = 0 ; rgb[2] < MaxRGB ; rgb[2]+=512 ) {
				double lab[3];
				RGB_to_CIELab(rgb,lab);
				int x = lab[1]*2.56+256.;
				int y = lab[2]*2.56+256.;
				if ( x < 0 ) x = 0 ; else if ( x >= 512 ) x = 511;
				if ( y < 0 ) y = 0 ; else if ( y >= 512 ) y = 511;
				image.pixelColor(x,y,Color(0,0,0,0));
			}
		*/
                        //calcul de la puissance de largeur
                        double theta = M_PI - M_PI * (mod("width").tof()/2) / 180.;
                        double puissance = (log(-M_LN2/(log(-(cos(theta)-1.)/2.)))+2.*M_LN2)/(M_LN2);
                        puissance = pow(2.,puissance);

                        double bias_sat = mod("bias_sat").tof();
                        //double bias_val = mod("bias_val").tof();
                        bool inv_sat = mod("inv_sat").toi();
                        //bool inv_val = mod("inv_val").toi();


                        //calcul de l'angle d'application
                        theta = M_PI * mod("angle").tof()/180.;
        
                        for ( int i = 0 ; i < 0x10000 ; i+=4 ) {
                                double t = 2*M_PI*i/0x10000;
                                
                                double  mul;
                                if ( inv_sat )
                                        mul = bias_sat+(1.-pow((1.-cos(t+theta))/2.,puissance));
                                else
                                        mul = bias_sat+pow((1.-cos(t+theta))/2.,puissance);

				int	x = 256. + 127.*cos(t),
					y = 256. + 127.*sin(t),
					c;

				Color col = image.pixelColor(x,y);
				if ( col.redQuantum() < 5 ) c = MaxRGB; else c = 0;

                               image.pixelColor(x,y,Color(c,c,c,0));

                                        x = 256. - mul*127.*cos(t),
                                        y = 256. - mul*127.*sin(t);

                                //if ( x > 511 ) x = 511; else if ( x < 0 ) x = 0;
                                //if ( y > 511 ) y = 511; else if ( y < 0 ) y = 0;
				
				col = image.pixelColor(x,y);
				if ( col.redQuantum() < 5 ) c = MaxRGB; else c = 0;

                                image.pixelColor(x,y,Color(c,c,c,0));
                        }

		}
		else {
                        selectiveLabOp(image);
                        selectiveLabOp(curve);
                }

		iGamma(image,imageInfo.gamma,imageInfo.non_linear_range,false); // linear -> g
		iGamma(curve,imageInfo.gamma,imageInfo.non_linear_range,false); // linear -> g
        }

    };

    class Modulate : public Operator {
        public:
        Modulate(int i, ptr<Operator> lop) : Operator("modulate",i,lop) {
                mod("hue","0");
                mod("saturation","100");
                mod("value","100");
		mod("gamma","2.2");
                mod("lab","1");
                serial=0;
        }


	void labModulate(Image& image) {
           	     int h = image.rows(),
                	        w = image.columns();

	                image.modifyImage();
	                Pixels pixel_cache(image);

			double hue = M_PI*mod("hue").tof()/180.;
			double saturation = mod("saturation").tof()/100.;
			double value = mod("value").tof()/100.;

			saturation = ( 8.5*saturation + 1.5*value*saturation )/10.;

		#pragma omp parallel for
        	        for ( int y = 0 ; y < h ; ++y ) {
        	                PixelPacket *pixels = pixel_cache.get(0,y,w,1);
        	                if ( !pixels ) continue;
        	                double lab[w][3];

      	                for ( int x = 0 ; x < w ; ++x ) {
  					int rgb[3]={pixels[x].red,pixels[x].green,pixels[x].blue};

					RGB_to_LinearLab(rgb,lab[x]);
                                }
					//value

                                if ( value != 1. ) for ( int x = 0 ; x < w ; ++x ) {
        				/*double v= lab_linearize(lab[x][0]);
	        			v *= value;
                			lab[x][0] = lab_gammaize(v);
                			*/
                			lab[x][0] *= value;
                                }
					//sat

				if ( saturation != 1. ) for ( int x = 0 ; x < w ; ++x ) {
        				lab[x][1]*=saturation;
	        			lab[x][2]*=saturation;
	        		}

					//hue
				if ( hue != 0. ) for ( int x = 0 ; x < w ; ++x ) {
        				double module = sqrt(lab[x][1]*lab[x][1]+lab[x][2]*lab[x][2]);
	        			double arg ;//= lab[x][2]/fabs(lab[x][2])*M_PI/2-atan(lab[x][1]/lab[x][2]);
	        			if ( lab[x][2] > 0 ) 
	        			        arg = M_PI/2-atan(lab[x][1]/lab[x][2]);
	        			else
	        			        arg = -M_PI/2-atan(lab[x][1]/lab[x][2]);
	        			lab[x][1]=module * cos(arg+hue);
	        			lab[x][2]=module * sin(arg+hue);
                                }
                                
                                for ( int x = 0 ; x < w ; ++x ) {
				        int rgb[3];
				        LinearLab_to_RGB(lab[x],rgb);
					pixels[x].red=rgb[0];
					pixels[x].green=rgb[1];
					pixels[x].blue=rgb[2];
        	                }
                	}
        	#pragma omp barrier
	                pixel_cache.sync();
	}

        void doJob() {
                Profile prof("Modulate");
                bool lab=mod("lab").toi();
                double value=mod("value").tof();
                double saturation=mod("saturation").tof();
                double hue=mod("hue").tof();
                
                if ( value == 100. && saturation == 100. && hue == 0. ) return;                
                
		iGamma(image,imageInfo.gamma,imageInfo.non_linear_range,true); // g -> linear
		iGamma(curve,imageInfo.gamma,imageInfo.non_linear_range,true); // g -> linear
                if (!lab) {
                    image.modulate(value,
                                   saturation,
                                   (hue/1.8)+100.
                                   );
                    curve.modulate(value,
                                   saturation,
                                   (hue/1.8)+100.
                                   );
		}
                else {

			labModulate(image);
			labModulate(curve);

                }
		iGamma(image,imageInfo.gamma,imageInfo.non_linear_range,false); // linear -> g
		iGamma(curve,imageInfo.gamma,imageInfo.non_linear_range,false); // linear -> g
        }
    };

	class ShadowDesaturate : public Operator {
		public:
		ShadowDesaturate(int i, ptr<Operator> lop) : Operator("shadowdesaturate",i,lop) {
			mod("high","5");
			mod("range","3");
			mod("saturation","0");
			serial=0;
		}

		bool equals(double x, double y, double prec=0.00001) {

			return fabs(x-y) < prec;
		}

		void doShadowDesaturate(Image& image,double *lut) {
           	     int h = image.rows(),
                	        w = image.columns();

	                image.modifyImage();
	                Pixels pixel_cache(image);
			double maxl=0,maxa=0,maxb=0;
			//	        #pragma omp parallel for
        	        for ( int y = 0 ; y < h ; ++y ) {
        	                PixelPacket *pixels = pixel_cache.get(0,y,w,1);
        	                if ( !pixels ) continue;
        	                for ( int x = 0 ; x < w ; ++x ) {
#if 0
					int g = (0.299*pixels[x].red+0.587*pixels[x].green+0.114*pixels[x].blue)  ;
/*					double h=0.,s=0.,l=0.;
					RGB2HSL(pixels[x].red,pixels[x].green,pixels[x].blue,h,s,l);
					s=s*lut[g];
					HSL2RGB(h,s,l,pixels[x].red,pixels[x].green,pixels[x].blue);
*/

					if ( ! equals(lut[g],1.) ) {
						int rgb[3]; rgb[0]=pixels[x].red; rgb[1]=pixels[x].green; rgb[2]=pixels[x].blue;
						double lab[3];
						RGB_to_CIELab(rgb,lab);
						lab[1]*=lut[g];
						lab[2]*=lut[g];
						CIELab_to_RGB(lab,rgb);
						pixels[x].red=rgb[0]; pixels[x].green=rgb[1]; pixels[x].blue=rgb[2];
					}
#else
					int rgb[3]; rgb[0]=pixels[x].red; rgb[1]=pixels[x].green; rgb[2]=pixels[x].blue;
					double lab[3];
					RGB_to_LinearLab(rgb,lab);
					if ( lab[0] > maxl) maxl=lab[0];
					if ( lab[1] > maxa) maxa=lab[1];
					if ( lab[2] > maxb) maxb=lab[2];
					int L=lab[0]*MaxRGB;if ( L > (signed)MaxRGB ) L=MaxRGB;
					if ( ! equals(lut[L],1.) )
					  {
					    lab[1]*=lut[L];
					    lab[2]*=lut[L];
					    LinearLab_to_RGB(lab,rgb);
					    pixels[x].red=rgb[0]; pixels[x].green=rgb[1]; pixels[x].blue=rgb[2];
					  }
#endif

        	                }
                	}
			//        	#pragma omp barrier
	                pixel_cache.sync();
			Logger lo("desha");
			lo("maxl = " + ftostring(maxl));
			lo("maxa = " + ftostring(maxa));
			lo("maxb = " + ftostring(maxb));
		}

		void doJob() {
                        Profile prof("ShadowDesaturate");


			double high = mod("high").tof(),
				low = high+mod("range").tof();
				
			double saturation = mod("saturation").tof()/100.;


			double lut[MaxRGB+1];

			double threshold_low = pow(2.,16.-low);
			double threshold_high = pow(2.,16.-high);

			Logger l("desha");
			l("th low  ="+ftostring(threshold_low));
			l("th high ="+ftostring(threshold_high));
			for ( unsigned int i = 0 ; i < MaxRGB+1 ; ++i ) {
				if ( i < threshold_low ) {
					lut[i]=saturation;
				}
				else if ( i > threshold_high ) {
					lut[i]=1.;
				}
				else {
					//lut[i] = (sin(M_PI/(low-high)*(high+log(i/double(MaxRGB))/log(2))+M_PI/2.)+1.)/2.;
					lut[i] = ( saturation - 1. ) * (1.-(sin(M_PI/(low-high)*(high+log(i/double(MaxRGB))/log(2))+M_PI/2.)+1.)/2.) + 1.;
				}
			}


			iGamma(image,imageInfo.gamma,imageInfo.non_linear_range,true); // g -> linear
			doShadowDesaturate(image,lut);
			iGamma(image,imageInfo.gamma,imageInfo.non_linear_range,false); // linear -> g

			iGamma(curve,imageInfo.gamma,imageInfo.non_linear_range,true); // g -> linear
			doShadowDesaturate(curve,lut);
			iGamma(curve,imageInfo.gamma,imageInfo.non_linear_range,false); // linear -> g


		}
	};


    class MedianFilter : public Operator {
        public:
        MedianFilter(int i, ptr<Operator> lop) : Operator("medianfilter",i,lop) {
                mod("radius","0.1");
                serial=0;
        }
        void doJob() {
                Profile prof("MedianFilter");
                image.medianFilter(mod("radius").tof());
        }
    };

    class OilPaint : public Operator {
        public:
        OilPaint(int i, ptr<Operator> lop) : Operator("oilpaint",i,lop) {
                mod("radius","3");
                serial=0;
        }
        void doJob() {
                Profile prof("OilPaint");
                image.oilPaint(mod("radius").toi());
        }
    };

}

class Process : public Object {

        Mutex crit;
    public:
        String root;
        String tmp;
        String filename;
        String realpath;
        Vector<ptr<op::Operator> > operators;
        //Image original;
        //Image workingCopy;
        int upTo;
        int maxsize;
        int lastOperator;
    public:

    Process(const String& root_, const String& tmp_, const String& fn)
        : crit(), root(root_), tmp(tmp_), filename(fn), realpath(getRealPath(fn)),
           operators() , upTo(0) ,maxsize(0), lastOperator(0) {
        if ( isRaw(root+realpath) )
                operators.push_back(new op::LoadRaw(root,tmp,realpath));
        else
                operators.push_back(new op::Load(root,tmp,realpath));
    }

    void addOperator(const String& opstr, int opid=-1) {
        ptr<op::Operator> lastOp = NULL;
        if ( ! operators.empty() )
                 lastOp = operators.back();

        ptr<op::Operator> oper = NULL;

        if ( opid == -1 )
                opid = lastOperator+1;

        if ( opstr == "load" ) {
                oper = new op::Load("","","");
        }
        else if ( opstr == "loadraw" ) {
                oper = new op::LoadRaw("","","");
        }
        else if ( opstr == "negate" ) {
                oper = new op::Negate(opid,lastOp);
        }
        else if ( opstr == "bw" ) {
                oper = new op::BW(opid,lastOp);
        }
        else if ( opstr == "microcontrasts" ) {
                oper = new op::MicroContrasts(opid,lastOp);
        }
        else if ( opstr == "flip" ) {
                oper = new op::Flip(opid,lastOp);
        }
        else if ( opstr == "flop" ) {
                oper = new op::Flop(opid,lastOp);
        }
        else if ( opstr == "rotatecw" ) {
                oper = new op::RotateCW(opid,lastOp);
        }
        else if ( opstr == "rotateccw" ) {
                oper = new op::RotateCCW(opid,lastOp);
        }
        else if ( opstr == "unsharpmask" ) {
                oper = new op::UnsharpMask(opid,lastOp);
        }
        else if ( opstr == "contrast" ) {
                oper = new op::Contrast(opid,lastOp);
        }
        else if ( opstr == "sigmoidalcontrast" ) {
                oper = new op::SigmoidalContrast(opid,lastOp);
        }
        else if ( opstr == "sinuscontrast" ) {
                oper = new op::SinusContrast(opid,lastOp);
        }
        else if ( opstr == "softdr" ) {
                oper = new op::SoftDR(opid,lastOp);
        }
        else if ( opstr == "enhance" ) {
                oper = new op::Enhance(opid,lastOp);
        }
        else if ( opstr == "despeckle" ) {
                oper = new op::Despeckle(opid,lastOp);
        }
        else if ( opstr == "adaptive_threshold" ) {
                oper = new op::AdaptiveThreshold(opid,lastOp);
        }
        else if ( opstr == "reduce_noise" ) {
                oper = new op::ReduceNoise(opid,lastOp);
        }
        else if ( opstr == "hotpixels" ) {
                oper = new op::HotPixels(opid,lastOp);
        }
        else if ( opstr == "segment" ) {
                oper = new op::Segment(opid,lastOp);
        }
        else if ( opstr == "equalize" ) {
                oper = new op::Equalize(opid,lastOp);
        }
        else if ( opstr == "normalize" ) {
                oper = new op::Normalize(opid,lastOp);
        }
        else if ( opstr == "igamma" ) {
                oper = new op::IGamma(opid,lastOp);
	}
        else if ( opstr == "gamma" ) {
                oper = new op::Gamma(opid,lastOp);
        }
        else if ( opstr == "rgbgamma" ) {
                oper = new op::RGBGamma(opid,lastOp);
        }
        else if ( opstr == "histogram" ) {
                oper = new op::Histogram(opid,lastOp);
        }
        else if ( opstr == "lca" ) {
                oper = new op::LCA(opid,lastOp);
        }
        else if ( opstr == "showcurve" ) {
                oper = new op::ShowCurve(opid,lastOp);
        }
        else if ( opstr == "linearize" ) {
                oper = new op::Linearize(opid,lastOp);
        }
        else if ( opstr == "zonesystem" ) {
                oper = new op::ZoneSystem(opid,lastOp);
        }
        else if ( opstr == "dynamic_range" ) {
                oper = new op::DynamicRange(opid,lastOp);
        }
        else if ( opstr == "level" ) {
                oper = new op::Level(opid,lastOp);
        }
        else if ( opstr == "lines" ) {
                oper = new op::Lines(opid,lastOp);
        }
        else if ( opstr == "colorfilter" ) {
                oper = new op::ColorFilter(opid,lastOp);
        }
        else if ( opstr == "whitebalance" ) {
                oper = new op::WhiteBalance(opid,lastOp);
        }
        else if ( opstr == "charcoal" ) {
                oper = new op::Charcoal(opid,lastOp);
        }
        else if ( opstr == "blur" ) {
                oper = new op::Blur(opid,lastOp);
        }
        else if ( opstr == "gaussianblur" ) {
                oper = new op::GaussianBlur(opid,lastOp);
        }
        else if ( opstr == "emboss" ) {
                oper = new op::Emboss(opid,lastOp);
        }
        else if ( opstr == "implode" ) {
                oper = new op::Implode(opid,lastOp);
        }
        else if ( opstr == "selectivelabop" ) {
                oper = new op::SelectiveLabOp(opid,lastOp);
        }
        else if ( opstr == "modulate" ) {
                oper = new op::Modulate(opid,lastOp);
        }
        else if ( opstr == "shadowdesaturate" ) {
                oper = new op::ShadowDesaturate(opid,lastOp);
        }
        else if ( opstr == "medianfilter" ) {
                oper = new op::MedianFilter(opid,lastOp);
        }
        else if ( opstr == "oilpaint" ) {
                oper = new op::OilPaint(opid,lastOp);
        }


        else
                throw raii::Exception(("unknown operator: "+opstr).c_str());

        lastOperator=opid;
        upTo=lastOperator;
        operators.push_back(oper);

    }

    ptr<op::Operator> getOperator(int opId) {
        for ( Vector<ptr<op::Operator> >::iterator op = operators.begin();
                op != operators.end();
                ++op ) {
                if ( (*op)->getId() ==  opId ) {
                        return *op;
                }
        }
        return NULL;
    }

    void deleteOperator(int opId) {
        if ( opId == 0 ) raii::Exception("interdit");
        ptr<op::Operator> lop=* operators.begin();
        for ( Vector<ptr<op::Operator> >::iterator op = operators.begin();
                op != operators.end();
                ++op ) {
                if ( (*op)->getId() ==  opId ) {
                        //Magick::Image *lm=(*op)->propagate();
                        Vector<ptr<op::Operator> >::iterator next = operators.erase(op);
                        if ( next != operators.end() ) {
                                (*next)->lastOp=lop;
                                (*next)->outDate();
                        }
                        break;
                }
            lop=*op;
        }
    }

    void getBlob(int newmaxsize,Magick::Blob * blob) {

        Lock l1(crit);
        if ( newmaxsize != maxsize ) {
                maxsize=newmaxsize;
                ptr<op::Operator> loader = operators.front();
                loader->mod("maxsize",itostring(maxsize));
        }

        bool force=false;
        ptr<op::Operator> lop=NULL;
        for ( Vector<ptr<op::Operator> >::iterator op = operators.begin();
              op != operators.end();
              ++op ) {
              lop=(*op);
              force=lop->apply(force);
              if ( lop->getId() == upTo )
                 break;
        }
        try {
            Magick::Image *img = lop->getImage();
            img->quality(95);
            img->interlaceType(LineInterlace);
            img->write(blob,"JPG");
        } catch (...) {
                throw raii::Exception("ici");
        }
    }

    void deleteFrom(Connection& conn, String fn="") {
        if ( fn == "" ) fn=filename;
        conn.query("DELETE FROM gallery_process where filename = '"+path_encode(fn)+"'");
        conn.query("DELETE FROM gallery_process_ts where filename = '"+path_encode(fn)+"'");
    }

        //return true si existe en base
    bool loadFrom(Connection& conn, const String& fn="") {

        ResultSet rs=conn.query("SELECT * FROM gallery_process WHERE filename = '"+path_encode(fn==""?filename:fn)+"' ORDER BY opid,opserial");

        if ( rs.rowCount() == 0 ) return false;

        ptr<op::Operator> ope = getOperator(0);
        String tmp = ope->mod("tmp");
        String root = ope->mod("root");

        if ( fn != "" && fn != "__default__" ) filename=fn;

        operators.clear();
        int lastop=-1;

        while ( rs.next() ) {
                int upto = rs["upto"].toi();
                int opid = rs["opid"].toi();
                int opserial = rs["opserial"].toi();
                bool enabled = rs["enabled"].toi();
                String opname = url_decode(rs["opname"]);
                String param = url_decode(rs["param"]);
                String value = url_decode(rs["value"]);

                if ( opid != lastop ) {
                        addOperator(opname,opid);
                        if ( opid == 0 ) {
                                ptr<op::Operator> loader=getOperator(0);
                                loader->mod("tmp",tmp);
                                loader->mod("root",root);
                                loader->mod("filename",filename);
                        }
                        lastop=opid;
                }

                ope = getOperator(opid);
                upTo=upto;
                if ( enabled ) ope->enable(); else ope->disable();
                ope->mod(param,value);
                ope->serial=opserial;
        }

        return true;
    }

    void saveTo(Connection& conn, String fn="") {
        if ( fn == "" ) fn=filename;
        deleteFrom(conn,fn);

        ptr<op::Operator> lop=NULL;
        for ( Vector<ptr<op::Operator> >::iterator op = operators.begin();
                op != operators.end();
                ++op ) {

                lop=(*op);
                bool empty_param_list=true;

                for ( Map<String,String>::iterator param = lop->param.begin();
                 param != lop->param.end();
                 ++param ) {
                        // j'écarte les paramêtres filename et tmp systématiquement
                        if (  param->first != "filename" && param->first != "tmp" && param->first != "root" ) {
                                empty_param_list=false;
                                conn.query(
                                "INSERT INTO gallery_process VALUES ('"+path_encode(fn)+"',"+itostring(upTo)+","+itostring(lop->id)
                                +","+itostring(lop->serial)+","+(lop->enabled?"1":"0")+",'"+url_encode(lop->name)
                                +"','"+url_encode(param->first)+"','"+url_encode(param->second)+"')");
                        }
                 }
                 if ( empty_param_list ) {
                        conn.query(
                        "INSERT INTO gallery_process VALUES ('"+path_encode(fn)+"',"+itostring(upTo)+","+itostring(lop->id)
                        +","+itostring(lop->serial)+","+(lop->enabled?"1":"0")+",'"+url_encode(lop->name)
                        +"','nop','nop')");
                 }

        }

        conn.query("INSERT INTO gallery_process_ts VALUES ('"+path_encode(fn)+"',"+itostring(time(NULL))+")");
    }

    String getFilename() { return filename; }


};

class refreshImage {
        String target;
        int serial;
        public:
        refreshImage() : target("preview"), serial(time(NULL)) {}
        void setTarget(const String& str) { target=str; }
        void setSerial(int i) { serial=i; }
        void doStart(HttpServletRequest& request, HttpServletResponse& response) {


	String url = request.getContextPath() + "/edit.C" + request.getPathInfo() + "?action=show_process&serial="+ itostring(serial);
	String t="document.getElementById('"+target+"')";

        response << "$('loadingimg').style.top = parseInt( parseInt(parseInt("<<t<<".style.height) - 16) / 2) + 'px';\n";
	response << "$('loadingimg').style.left = parseInt( parseInt(parseInt("<<t<<".style.width) - 16) / 2) + 'px';\n";
	  response << "$('loadingimg').style.display='block';\n";
          response << t << ".src='"<<url<<"';\n";

	  response << t <<".onload=function(e) { "<<t<<".onload=null; $('loadingimg').style.display='none'; } ";

        }
};
