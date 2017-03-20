#if defined(ARDUINO) && ARDUINO >= 100
#include "Arduino.h"
#else
#include "WProgram.h"
#endif

#include "AWI_Color.h"



// empty constructor
AWI_Color::AWI_Color(){
}

/*
// get Fastled CRGB from color temperature in Mired
void AWI_Color::getRGBfromTemperature(CRGB& convRGB, double colorTemp){
// http://www.tannerhelland.com/4435/convert-temperature-rgb-algorithm-code/
// HUE uses:  mired = 1.000.000/tmpKelvin
    double tmpKelvin = 1000000.0 / colorTemp ;				// Hue conversion from mired
	double tmpCalc ;
	// Sprint("colorTemp(mired): ") ; Sprint(colorTemp) ; Sprint("  Kelvin: ") ; Sprintln(tmpKelvin) ;
	// Temperature must fall between 1000 and 40000 degrees
    if (tmpKelvin < 1000) tmpKelvin = 1000 ;
    if (tmpKelvin > 40000)tmpKelvin = 40000 ;
    
    // All calculations require tmpKelvin \ 100, so only do the conversion once
    tmpKelvin = tmpKelvin / 100.0 ;
    
    //Calculate each color in turn
    
	// First: red
    if (tmpKelvin <= 66){
        convRGB.r = 255 ;
	} else {
        // Note: the R-squared value for this approximation is .988
        tmpCalc = tmpKelvin - 60 ;
        tmpCalc = 329.698727446 * pow(tmpCalc, -0.1332047592) ;
        if (tmpCalc < 0) tmpCalc = 0 ;
        if (tmpCalc > 255)tmpCalc = 255 ;
        convRGB.r = tmpCalc ;
		}
    
    // Second: green
    if (tmpKelvin <= 66) {
        // Note: the R-squared value for this approximation is .996
        tmpCalc = tmpKelvin ;
        tmpCalc = 99.4708025861 * log(tmpCalc) - 161.1195681661 ;
        if (tmpCalc < 0) tmpCalc = 0 ;
        if (tmpCalc > 255) tmpCalc = 255 ;
		convRGB.g = tmpCalc ;
 	} else {
        // Note: the R-squared value for this approximation is .987
        tmpCalc = tmpKelvin - 60 ;
        tmpCalc = 288.1221695283 * pow(tmpCalc, -0.0755148492) ;
        if (tmpCalc < 0) tmpCalc = 0 ;
        if (tmpCalc > 255)tmpCalc = 255 ;
        convRGB.g = tmpCalc ;
	}
    
    // Third: blue
    if (tmpKelvin >= 66){
        convRGB.b = 255 ;
	} else if (tmpKelvin <= 19){
		convRGB.b = 0 ;
	} else {
		// Note: the R-squared value for this approximation is .998
		tmpCalc = tmpKelvin - 10 ;
		tmpCalc = 138.5177312231 * log(tmpCalc) - 305.0447927307 ;
		if (tmpCalc < 0) tmpCalc = 0 ;
		if (tmpCalc > 255) tmpCalc = 255 ;
		convRGB.b = tmpCalc ;
	}
}
*/

void AWI_Color::getRGBfromXY(CRGB& convRGB, double x, double y, double bri) {
// https://developers.meethue.com/documentation/color-conversions-rgb-xy

	// Check if in reach of light
    // : not implemented
	double z = 1.0f - x - y;

    double Y = bri;
    double X = (Y / y) * x;
    double Z = (Y / y) * z;

    // sRGB D65 conversion
    double r =  X * 1.656492f - Y * 0.354851f - Z * 0.255038f;
    double g = -X * 0.707196f + Y * 1.655397f + Z * 0.036152f;
    double b =  X * 0.051713f - Y * 0.121364f + Z * 1.011530f;

    if (r > b && r > g && r > 1.0f) {
        // red is too big
        g = g / r;
        b = b / r;
        r = 1.0f;
    }
    else if (g > b && g > r && g > 1.0f) {
        // green is too big
        r = r / g;
        b = b / g;
        g = 1.0f;
    }
    else if (b > r && b > g && b > 1.0f) {
        // blue is too big
        r = r / b;
        g = g / b;
        b = 1.0f;
    }

    // Apply gamma correction
    r = r <= 0.0031308f ? 12.92f * r : (1.0f + 0.055f) * pow(r, (1.0f / 2.4f)) - 0.055f;
    g = g <= 0.0031308f ? 12.92f * g : (1.0f + 0.055f) * pow(g, (1.0f / 2.4f)) - 0.055f;
    b = b <= 0.0031308f ? 12.92f * b : (1.0f + 0.055f) * pow(b, (1.0f / 2.4f)) - 0.055f;

    if (r > b && r > g) {
        // red is biggest
        if (r > 1.0f) {
            g = g / r;
            b = b / r;
            r = 1.0f;
        }
    }
    else if (g > b && g > r) {
        // green is biggest
        if (g > 1.0f) {
            r = r / g;
            b = b / g;
            g = 1.0f;
        }
    }
    else if (b > r && b > g) {
        // blue is biggest
        if (b > 1.0f) {
            r = r / b;
            g = g / b;
            b = 1.0f;
        }
    }
	convRGB.r = r * 255 ;
	convRGB.g = g * 255 ;
	convRGB.b = b * 255 ;
	
    return ;
}

void AWI_Color::getXYfromRGB(double& cx, double& cy, double& bri, CRGB& convRGB) {
// https://developers.meethue.com/documentation/color-conversions-rgb-xy
	double red = (float)convRGB.r /255.0 ;
	double green = (float)convRGB.g /255.0 ;
	double blue = (float)convRGB.b /255.0;

    // Apply gamma correction
    double r = (red   > 0.04045f) ? pow((red   + 0.055f) / (1.0f + 0.055f), 2.4f) : (red   / 12.92f);
    double g = (green > 0.04045f) ? pow((green + 0.055f) / (1.0f + 0.055f), 2.4f) : (green / 12.92f);
    double b = (blue  > 0.04045f) ? pow((blue  + 0.055f) / (1.0f + 0.055f), 2.4f) : (blue  / 12.92f);

    // Wide gamut conversion D65
    double X = r * 0.664511f + g * 0.154324f + b * 0.162028f;
    double Y = r * 0.283881f + g * 0.668433f + b * 0.047685f;
    double Z = r * 0.000088f + g * 0.072310f + b * 0.986039f;

    cx = X / (X + Y + Z);
    cy = Y / (X + Y + Z);

    if (isnan(cx)) {
        cx = 0.0f;
    }

    if (isnan(cy)) {
        cy = 0.0f;
    }
	bri = Y ;			// brightness
}


void AWI_Color::getRGBfromHSV( CRGB& convRGB, CHSV& convHSV ) {
// fastled, https://github.com/FastLED/FastLED/blob/master/hsv2rgb.h
	hsv2rgb_rainbow(convHSV, convRGB) ;
    return ;
}

void AWI_Color::getHSVfromRGB(CHSV& convHSV, CRGB& convRGB) {
// fastled, https://github.com/FastLED/FastLED/blob/master/hsv2rgb.h
	convHSV = rgb2hsv_approximate(convRGB);
    return ;
}

/* Convert between Temperature and RGB.
 * Base on information from http://www.brucelindbloom.com/
 * The fit for D-illuminant between 4000K and 23000K are from CIE
 * The generalization to 2000K < T < 4000K and the blackbody fits
 * are my own and should be taken with a grain of salt.
 *
static const double XYZ_to_RGB[3][3] = {
	{ 3.24071,  -0.969258,  0.0556352 },
	{ -1.53726, 1.87599,    -0.203996 },
	{ -0.498571, 0.0415557,  1.05707 }
};
*/

// get Fastled CRGB from color temperature in Mired
// colorTemp value (~100 - ~600 mired)(1.666 - 10.000K)
void AWI_Color::getRGBfromTemperature(CRGB& convRGB, double colorTemp){
	double RGB[3] ;
	double temp = 1000000.0/ colorTemp ; 					// convert from mired to Kelvin
	AWI_Color::Temperature_to_RGB(temp, RGB) ;
	convRGB.r = RGB[0] * 255.0 ; 
	convRGB.g = RGB[1] * 255.0 ;
	convRGB.b = RGB[2] * 255.0 ;
}

// get color temperature mired from Fastled CRGB
// colorTemp value (~100 - ~600 mired)(=1.666 - 10.000K)
// ("green" is indicator for accuracy, not exhibited)
void AWI_Color::getTemperatureFromRGB(double& colorTemp, CRGB& convRGB /*, double& green */ ){
	double RGB[3], temp, green ; 
	RGB[0] = (float)convRGB.r / 255.0 ;
	RGB[1] = (float)convRGB.g / 255.0 ;
	RGB[2] = (float)convRGB.b / 255.0 ;
	AWI_Color::RGB_to_Temperature(RGB, &temp, &green) ;
	colorTemp = 1000000.0/ temp ; 						// convert from Kelvin to mired
}

// private functions for temp RGB conversion (different units, Kelvin and double RGB (0.000....1.00))
void AWI_Color::Temperature_to_RGB(double T, double RGB[3]){
	int c;
	double xD, yD, X, Y, Z, max;
	// Fit for CIE Daylight illuminant
	if (T <= 4000) {
		xD = 0.27475e9 / (T * T * T) - 0.98598e6 / (T * T) + 1.17444e3 / T + 0.145986;
	} else if (T <= 7000) {
		xD = -4.6070e9 / (T * T * T) + 2.9678e6 / (T * T) + 0.09911e3 / T + 0.244063;
	} else {
		xD = -2.0064e9 / (T * T * T) + 1.9018e6 / (T * T) + 0.24748e3 / T + 0.237040;
	}
	yD = -3 * xD * xD + 2.87 * xD - 0.275;

	// Fit for Blackbody using CIE standard observer function at 2 degrees
	//xD = -1.8596e9/(T*T*T) + 1.37686e6/(T*T) + 0.360496e3/T + 0.232632;
	//yD = -2.6046*xD*xD + 2.6106*xD - 0.239156;

	// Fit for Blackbody using CIE standard observer function at 10 degrees
	//xD = -1.98883e9/(T*T*T) + 1.45155e6/(T*T) + 0.364774e3/T + 0.231136;
	//yD = -2.35563*xD*xD + 2.39688*xD - 0.196035;

	X = xD / yD;
	Y = 1;
	Z = (1 - xD - yD) / yD;
	max = 0;
	for (c = 0; c < 3; c++) {
		RGB[c] = X * XYZ_to_RGB[0][c] + Y * XYZ_to_RGB[1][c] + Z * XYZ_to_RGB[2][c];
		if (RGB[c] > max) max = RGB[c];
	}
	for (c = 0; c < 3; c++) RGB[c] = RGB[c] / max;
}

void AWI_Color::RGB_to_Temperature(double RGB[3], double *T, double *Green){
	double Tmax, Tmin, testRGB[3];
	Tmin = 2000;
	Tmax = 23000;
	for (*T = (Tmax + Tmin) / 2; Tmax - Tmin > 0.1; *T = (Tmax + Tmin) / 2) {
		AWI_Color::Temperature_to_RGB(*T, testRGB);
		if (testRGB[2] / testRGB[0] > RGB[2] / RGB[0])
			Tmax = *T;
		else
			Tmin = *T;
	}
	*Green = (testRGB[1] / testRGB[0]) / (RGB[1] / RGB[0]);
	if (*Green < 0.2) *Green = 0.2;
	if (*Green > 2.5) *Green = 2.5;
}

