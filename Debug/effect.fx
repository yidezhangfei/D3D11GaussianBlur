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
  float SampleWeights[9];
};

cbuffer cbSampleOffsets : register (b1)
{
  float2 SampleOffsets[9];
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
  
  //return texDiffuse.Sample(samLinear, input.Tex);
  
  for (uint k = 0; k < 9; k++)
  {
    color += texDiffuse.Sample(samLinear, input.Tex + SampleOffsets[k]) * SampleWeights[k];
  }

  return color;
}
