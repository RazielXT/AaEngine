// Pack float4 [-1..1] into signed R10G10B10A2 (SNORM)
uint PackRGB10A2_SNORM(float4 c)
{
    // Clamp to SNORM range
    c = clamp(c, -1.0, 1.0);

    // Convert to signed integers
    int r = (int)(c.r * 511.0 + (c.r >= 0 ? 0.5 : -0.5));   // 10-bit signed
    int g = (int)(c.g * 511.0 + (c.g >= 0 ? 0.5 : -0.5));
    int b = (int)(c.b * 511.0 + (c.b >= 0 ? 0.5 : -0.5));
    int a = (int)(c.a *   1.0 + (c.a >= 0 ? 0.5 : -0.5));   // 2-bit signed

    // Reinterpret as unsigned for packing
    uint ur = (uint)(r & 0x3FF);
    uint ug = (uint)(g & 0x3FF);
    uint ub = (uint)(b & 0x3FF);
    uint ua = (uint)(a & 0x003);

    return ur | (ug << 10) | (ub << 20) | (ua << 30);
}

// Unpack signed R10G10B10A2_SNORM back into float4 [-1..1]
float4 UnpackRGB10A2_SNORM(uint v)
{
    // Extract raw bits
    int r = (int)(v        & 0x3FF);
    int g = (int)((v >> 10) & 0x3FF);
    int b = (int)((v >> 20) & 0x3FF);
    int a = (int)((v >> 30) & 0x003);

    // Sign‑extend 10bit and 2bit values
    r = (r << 22) >> 22;   // 10bit sign extend
    g = (g << 22) >> 22;
    b = (b << 22) >> 22;
    a = (a << 30) >> 30;   // 2bit sign extend

    float4 c;
    c.r = r / 511.0;
    c.g = g / 511.0;
    c.b = b / 511.0;
    c.a = a /   1.0;
    return c;
}

uint PackR11G10B11_SNORM(float3 n)
{
    // Clamp to SNORM range
    n = clamp(n, -1.0, 1.0);

    // Convert to signed integers
    int xi = (int)(n.x * 1023.0 + (n.x >= 0 ? 0.5 : -0.5)); // 11 bits → [-1023..1023]
    int yi = (int)(n.y *  511.0 + (n.y >= 0 ? 0.5 : -0.5)); // 10 bits → [-511..511]
    int zi = (int)(n.z * 1023.0 + (n.z >= 0 ? 0.5 : -0.5)); // 11 bits → [-1023..1023]

    // Mask to bit widths
    uint ux = (uint)(xi & 0x7FF); // 11 bits
    uint uy = (uint)(yi & 0x3FF); // 10 bits
    uint uz = (uint)(zi & 0x7FF); // 11 bits

    return (ux << 21) | (uy << 11) | uz;
}

float3 UnpackR11G10B11_SNORM(uint p)
{
    // Extract
    int xi = (p >> 21) & 0x7FF; // 11 bits
    int yi = (p >> 11) & 0x3FF; // 10 bits
    int zi =  p        & 0x7FF; // 11 bits

    // Sign extend
    xi = (xi << 21) >> 21; // 11-bit sign extend
    yi = (yi << 22) >> 22; // 10-bit sign extend
    zi = (zi << 21) >> 21; // 11-bit sign extend

    float3 n;
    n.x = xi / 1023.0;
    n.y = yi /  511.0;
    n.z = zi / 1023.0;
    return n;
}


// Pack float4 [0..1] into RGBA8 uint
uint PackRGBA8(float4 c)
{
    uint4 u = (uint4)(saturate(c) * 255.0 + 0.5);
    return (u.r) | (u.g << 8) | (u.b << 16) | (u.a << 24);
}

// Unpack RGBA8 uint back into float4 [0..1]
float4 UnpackRGBA8(uint v)
{
    float4 c;
    c.r = (v        & 0xFF) / 255.0;
    c.g = ((v >> 8) & 0xFF) / 255.0;
    c.b = ((v >> 16) & 0xFF) / 255.0;
    c.a = ((v >> 24) & 0xFF) / 255.0;
    return c;
}
