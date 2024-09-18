
/*
** Photoshop & misc math
** Blending modes, RGB/HSL/Contrast/Desaturate
**
** Romain Dura | Romz
** Blog: http://blog.mouaif.org
** Post: http://blog.mouaif.org/?p=94
*/

/*
** Desaturation
*/

float4 Desaturate(float3 color, float Desaturation)
{
    float3 grayXfer = float3(0.3, 0.59, 0.11);
    float grayf = dot(grayXfer, color);
    float3 gray = float3(grayf, grayf, grayf);
    return float4(lerp(color, gray, Desaturation), 1.0);
}

/*
** Hue, saturation, luminance
*/

float3 RGBToHSL(float3 color)
{
    float3 hsl; // init to 0 to avoid warnings ? (and reverse if + remove first part)
	
    float fmin = min(min(color.r, color.g), color.b); //Min. value of RGB
    float fmax = max(max(color.r, color.g), color.b); //Max. value of RGB
    float delta = fmax - fmin; //Delta RGB value

    hsl.z = (fmax + fmin) / 2.0; // Luminance

    if (delta == 0.0)		//This is a gray, no chroma...
    {
        hsl.x = 0.0; // Hue
        hsl.y = 0.0; // Saturation
    }
    else //Chromatic data...
    {
        if (hsl.z < 0.5)
            hsl.y = delta / (fmax + fmin); // Saturation
        else
            hsl.y = delta / (2.0 - fmax - fmin); // Saturation
		
        float deltaR = (((fmax - color.r) / 6.0) + (delta / 2.0)) / delta;
        float deltaG = (((fmax - color.g) / 6.0) + (delta / 2.0)) / delta;
        float deltaB = (((fmax - color.b) / 6.0) + (delta / 2.0)) / delta;

        if (color.r == fmax)
            hsl.x = deltaB - deltaG; // Hue
        else if (color.g == fmax)
            hsl.x = (1.0 / 3.0) + deltaR - deltaB; // Hue
        else if (color.b == fmax)
            hsl.x = (2.0 / 3.0) + deltaG - deltaR; // Hue

        if (hsl.x < 0.0)
            hsl.x += 1.0; // Hue
        else if (hsl.x > 1.0)
            hsl.x -= 1.0; // Hue
    }

    return hsl;
}

float HueToRGB(float f1, float f2, float hue)
{
    if (hue < 0.0)
        hue += 1.0;
    else if (hue > 1.0)
        hue -= 1.0;
    float res;
    if ((6.0 * hue) < 1.0)
        res = f1 + (f2 - f1) * 6.0 * hue;
    else if ((2.0 * hue) < 1.0)
        res = f2;
    else if ((3.0 * hue) < 2.0)
        res = f1 + (f2 - f1) * ((2.0 / 3.0) - hue) * 6.0;
    else
        res = f1;
    return res;
}

float3 HSLToRGB(float3 hsl)
{
    float3 rgb;
	
    if (hsl.y == 0.0)
        rgb = float3(hsl.z, hsl.z, hsl.z); // Luminance
    else
    {
        float f2;
		
        if (hsl.z < 0.5)
            f2 = hsl.z * (1.0 + hsl.y);
        else
            f2 = (hsl.z + hsl.y) - (hsl.y * hsl.z);
			
        float f1 = 2.0 * hsl.z - f2;
		
        rgb.r = HueToRGB(f1, f2, hsl.x + (1.0 / 3.0));
        rgb.g = HueToRGB(f1, f2, hsl.x);
        rgb.b = HueToRGB(f1, f2, hsl.x - (1.0 / 3.0));
    }
	
    return rgb;
}

/*
** Contrast, saturation, brightness
** Code of this function is from TGM's shader pack
** http://irrlicht.sourceforge.net/phpBB2/viewtopic.php?t=21057
*/

// For all settings: 1.0 = 100% 0.5=50% 1.5 = 150%
float3 ContrastSaturationBrightness(float3 color, float brt, float sat, float con)
{
	// Increase or decrease theese values to adjust r, g and b color channels seperately
    const float AvgLumR = 0.4;
    const float AvgLumG = 0.5;
    const float AvgLumB = 0.5;
	
    const float3 LumCoeff = float3(0.2125, 0.7154, 0.0721);
	
    float3 AvgLumin = float3(AvgLumR, AvgLumG, AvgLumB);
    float3 brtColor = color * brt;
    float intensityf = dot(brtColor, LumCoeff);
    float3 intensity = float3(intensityf, intensityf, intensityf);
    float3 satColor = lerp(intensity, brtColor, sat);
    float3 conColor = lerp(AvgLumin, satColor, con);
    return conColor;
}
