///--------------------------------------------------------------------------------------
// File: Tutorial02.fx
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

// Constant 
Texture2D texDiffuse : register (t0);
SamplerState samLinear : register (s0);

struct VS_INPUT
{
  float4 Pos : POSITION;
  float2 Tex : TEXCOORD0;
};

struct PS_INPUT
{
  float4 Pos : SV_POSITION;
  float2 Tex : TEXCOORD0;
};

cbuffer cbSampleWeights : register (b0)
{
  float4 SampleWeights[44];
};

cbuffer cbSampleOffsets : register (b1)
{
  float4 SampleOffsets[88];
};

cbuffer cbRadius : register (b2)
{
  uint radius;
};

//--------------------------------------------------------------------------------------
// Vertex Shader
//--------------------------------------------------------------------------------------
PS_INPUT VS(VS_INPUT input)
{
  PS_INPUT output = (PS_INPUT)0;
  output.Pos = input.Pos;
  output.Tex = input.Tex;

  return output;
}


//--------------------------------------------------------------------------------------
// Pixel Shader
//--------------------------------------------------------------------------------------
float4 PS(PS_INPUT input) : SV_Target
{
  float4 color = 0.0f;
  static float2 SampleOffsetsFloat[176] = (float2[176])SampleOffsets;
  static float SampleWeightsFloat[176] = (float[176])SampleWeights;
  
  //return texDiffuse.Sample(samLinear, input.Tex);
  
  for (uint k = 0; k < 49; k++)
  {
    color += texDiffuse.Sample(samLinear, input.Tex + SampleOffsetsFloat[k]) * SampleWeightsFloat[k];
  }
  //color = texDiffuse.Sample(samLinear, input.Tex);
  //color = float4(1.0, 0.0, 0.0, 1.0);
  return color;
}
