#include "FastLED.h"

class AWI_Color
{
	public:
		// empty constructor
		AWI_Color();

		// get Fastled CRGB from X, Y colorspace
		// https://developers.meethue.com/documentation/color-conversions-rgb-xy
		void getRGBfromXY(CRGB& convRGB, double x, double y, double bri);

		// get Fastled CRGB from HSV
		// fastled, https://github.com/FastLED/FastLED/blob/master/hsv2rgb.h
		void getRGBfromHSV( CRGB& convRGB, CHSV& convHSV );
		
		// https://developers.meethue.com/documentation/color-conversions-rgb-xy	
		void getXYfromRGB(double& cx, double& cy, double& bri, CRGB& convRGB);

		// fastled, https://github.com/FastLED/
		void getHSVfromRGB(CHSV& convHSV, CRGB& convRGB) ;

		// get Fastled CRGB from color temperature in Mired
		// colorTemp value (~100 - ~600 mired)(1.666 - 10.000K)
		void getRGBfromTemperature(CRGB& convRGB, double temp );

		// get color temperature mired from Fastled CRGB
		// colorTemp value (~100 - ~600 mired)( = 1.666 - 10.000K)
		void getTemperatureFromRGB(double& temp, CRGB& convRGB);
		
	private:
		/* Convert between Temperature and RGB.
		 * Base on information from http://www.brucelindbloom.com/
		 * The fit for D-illuminant between 4000K and 23000K are from CIE
		 * The generalization to 2000K < T < 4000K and the blackbody fits
		 * are my own and should be taken with a grain of salt.
		 */
		const double XYZ_to_RGB[3][3] = {
			{ 3.24071,  -0.969258,  0.0556352 },
			{ -1.53726, 1.87599,    -0.203996 },
			{ -0.498571, 0.0415557,  1.05707 }
		};

		void Temperature_to_RGB(double T, double RGB[3]) ;
		
		void RGB_to_Temperature(double RGB[3], double *T, double *Green) ;
};