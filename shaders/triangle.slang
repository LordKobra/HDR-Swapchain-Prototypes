struct PushConstants
{
  uint  colorspaceEnum;
  uint  formatEnum;
  uint  formatBits;
  uint  formatSrgb;
  float appMaxCLL;
  float appWhitepoint;
  bool  leftHanded;

  uint screenWidth;
  uint screenHeight;
}

[[vk::push_constant]]
PushConstants colorinfo; // 0 sRGB, 1 scRGB

struct VS_Out
{
  float4 sv_position : SV_Position;
  float2 uv : TEXCOORD0;
}

[shader("vertex")]
VS_Out vertMain(uint id: SV_VertexID)
{
  VS_Out output;
  output.uv          = 1.0;
  output.uv.x        = (id == 2) ? 2.0 : 0.0;
  output.uv.y        = (id == 1) ? 2.0 : 0.0;
  output.sv_position = float4(output.uv.xy * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);

  output.uv.y = (colorinfo.leftHanded) ? 1.0 - output.uv.y : output.uv.y;
  //  x0 -1,1 x1 3.0, 1.0 x2 -1.0, -3.0
  return output;
}

float3 xyYToRec709(float2 xy, float Y = 1.0)
{
  // https://github.com/ampas/aces-dev/blob/v1.0.3/transforms/ctl/README-MATRIX.md
  static const float3x3 XYZtoRGB   = { 3.2409699419,
                                       -1.5373831776,
                                       -0.4986107603,
                                       -0.9692436363,
                                       1.8759675015,
                                       0.0415550574,
                                       0.0556300797,
                                       -0.2039769589,
                                       1.0569715142 };
  float3                XYZ        = Y * float3(xy.x / xy.y, 1.0, (1.0 - xy.x - xy.y) / xy.y);
  float3                RGB        = mul(XYZtoRGB, XYZ);
  float                 maxChannel = max(RGB.r, max(RGB.g, RGB.b));
  return RGB / max(maxChannel, 1.0);
}

float3 rec2020_to_rec709(float3 rec2020)
{
  const float3x3 M_REC2020_TO_REC709 = float3x3(
      1.66049098, -0.58764111, -0.07284986, -0.12455047, 1.13289988, -0.00834942, -0.01815076, -0.10057889, 1.11872971);
  return mul(M_REC2020_TO_REC709, rec2020);
}
float3 rec709_to_rec2020(float3 rec709)
{
  const float3x3 M_REC709_TO_REC2020 = float3x3(
      0.62740391, 0.32928302, 0.04331306, 0.06909728, 0.91954040, 0.01136231, 0.01639143, 0.08801330, 0.89559525);
  return mul(M_REC709_TO_REC2020, rec709);
}
float3 linear_to_srgb(float3 c)
{
  // return pow(saturate(c), 1.0 / 2.2);
  return select(c < 0.0031308, c * 12.92, 1.055 * pow(c, (1.0 / 2.4)) - 0.055);
}

float3 linear_to_scrgb(float3 c)
{
  return c * colorinfo.appWhitepoint / 80.0;
}

float3 pq_inverse_eotf(float3 l) // linear -> compressed
{
  const float M1 = 2610.0 / 4096.0 * 0.25;
  const float M2 = 2523.0 / 4096.0 * 128.0;
  const float C1 = 3424.0 / 4096.0;
  const float C2 = 2413.0 / 4096.0 * 32.0;
  const float C3 = 2392.0 / 4096.0 * 32.0;
  return pow((C1 + C2 * pow(l, M1)) / (1.0 + C3 * pow(l, M1)), M2);
}

float3 linear_to_pq(float3 c)
{
  c = c * colorinfo.appWhitepoint / 10000.0; // requires input: SDR = 1.0
  return pq_inverse_eotf(c);
}

// does not catch boundaries
bool naivePointInTriangle(float2 p, float2 a, float2 b, float2 c)
{
  float denom = ((b.y - c.y) * (a.x - c.x) + (c.x - b.x) * (a.y - c.y));
  float v1    = ((b.y - c.y) * (p.x - c.x) + (c.x - b.x) * (p.y - c.y)) / denom;
  float v2    = ((c.y - a.y) * (p.x - c.x) + (a.x - c.x) * (p.y - c.y)) / denom;
  float v3    = 1.0 - v1 - v2;
  return 0 <= v1 && v1 <= 1 && 0 <= v2 && v2 <= 1 && 0 <= v3 && v3 <= 1;
}

void drawBrightness(float2 uv, inout float3 color)
{
  // draw srgb brightness
  if (uv.x > 0.05 && uv.x < 0.45 && uv.y > 0.7 && uv.y < 0.8)
  {
    color = (uv.x - 0.05) / 0.4 * colorinfo.appMaxCLL / colorinfo.appWhitepoint;
    color = (color.x > 1.0) ? 1.0 : color;
  }
  // draw srgb brightness
  if (uv.x > 0.05 && uv.x < 0.45 && uv.y > 0.45 && uv.y < 0.55)
  {
    color = (uv.x - 0.05) / 0.4 * colorinfo.appMaxCLL / colorinfo.appWhitepoint;
    color = (color.x > 1.0) ? 0.0 : color;
  }
  // draw hdr brightness
  if (uv.x > 0.05 && uv.x < 0.45 && uv.y > 0.2 && uv.y < 0.3)
  {
    color = (uv.x - 0.05) / 0.4 * colorinfo.appMaxCLL / colorinfo.appWhitepoint;
  }
}

void drawColorspaces(float2 uv, inout float3 color)
{

  // Rec2020 chromaticity coords
  float2 red   = float2(0.708, 0.292);
  float2 green = float2(0.170, 0.797);
  float2 blue  = float2(0.131, 0.046);

  // draw sdr rec2020
  if (uv.x > 0.5 && uv.x < 1.0 && uv.y > 0.5 && uv.y < 1.0)
  {
    float2 nc = (uv.xy - 0.5) / 0.5;

    if (naivePointInTriangle(nc, red, green, blue))
    {
      color = saturate(xyYToRec709(nc));
    }
  }

  // draw hdr rec2020
  if (uv.x > 0.5 && uv.x < 1.0 && uv.y > 0.0 && uv.y < 0.5)
  {
    float2 nc = float2((uv.x - 0.5) / 0.5, (uv.y) / 0.5);

    if (naivePointInTriangle(nc, red, green, blue))
    {
      color = xyYToRec709(nc);
    }
  }
}

[shader("fragment")]
float4 fragMain(VS_Out inVert)
    : SV_Target
{
  float3 color = 0.0f;

  // correct aspect ratio
  float  min_dim    = min(colorinfo.screenWidth, colorinfo.screenHeight);
  float2 ar_correct = float2(colorinfo.screenWidth, colorinfo.screenHeight) / min_dim;
  float2 ar_offset  = max(0.0, ar_correct - 1.0) / 2.0;

  drawBrightness(inVert.uv * ar_correct - ar_offset, color);
  drawColorspaces(inVert.uv * ar_correct - ar_offset, color);
  // color.xyz = ar_correct.x < 1.0;
  float4 rv = float4(color, 1.0);
  // assuming input is linear light with rec.709 primaries
  if (colorinfo.colorspaceEnum == 0 && colorinfo.formatSrgb == 0) // srgb _UNORM
  {
    rv.xyz = linear_to_srgb(color); // Note: Colorspace is not enough, we also need to know if the format is srgb or not
  }
  if (colorinfo.colorspaceEnum == 0 && colorinfo.formatSrgb != 0) // srgb _SRGB suffix
  {
    rv.xyz = color;
  }
  if (colorinfo.colorspaceEnum == 10) // hdr10 ST2084
  {
    rv.xyz = linear_to_pq(rec709_to_rec2020(color));
  }
  if (colorinfo.colorspaceEnum == 16) // scrgb
  {
    rv.xyz = linear_to_scrgb(color);
  }
  return rv;
}
