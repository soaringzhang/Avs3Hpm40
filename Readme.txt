/******************************************************************
        High-Performance Model(HPM) Software Manual
******************************************************************/
please send comments and additions to kuifan@pku.edu.cn

1. Compilation
2. Command line parameters
3. Examples of command line
4. Configuration files

****************************************************************
1. Compilation

1.1 Windows
  
  A workspace for MS Visual Studio is provided with the name "hpm_vsXXXX.sln" in the directory "build\x86_windows". It contains the encoder and decoder projects.

1.2 Unix/Linux

  Makefiles are provided in the directory "build\x86_linux".
  'make' command will create the obj files and generate the executable file in the 'bin' directory.
	
*******************************************************************
2. Command line parameters

  2.1 Encoder

     encoder_app [--config file] [-paramShort ParameterValue] [--paramLong ParameterValue]

     --config file    
             All Parameters are initially taken from the 'file', typically: "encode_RA.cfg".

     -paramShort ParameterValue
     --paramLong ParameterValue
             If -paramShort or --paramLong parameters are present, then the ParameterValue will override the default settings in the configuration file.

  2.2 Decoder

     decoder_app [-paramShort ParameterValue] [--paramLong ParameterValue]
	 
	 All decoding parameters are set by the command lines.

*******************************************************************
3. Examples of command line
     
  2.1 Random Access
     
     build\x86_windows\x64\Release\encoder_app --config cfg\encode_RA.cfg -i City_1280x720_60.yuv -w 1280 -h 720 -z 60 -p 64 -f 9 -d 8 -q 45 -o City_RA.bin -r City_RA_rec.yuv
     build\x86_windows\x64\Release\decoder_app -s -i City_RA.bin -o City_RA_dec.yuv
     
  2.2 All Intra
  
     build\x86_windows\x64\Release\encoder_app --config cfg\encode_AI.cfg -i City_1280x720_60.yuv -w 1280 -h 720 -z 60 -f 9 -d 8 -q 45 -o City_AI.bin -r City_AI_rec.yuv
     build\x86_windows\x64\Release\decoder_app -s -i City_AI.bin -o City_AI_dec.yuv

  2.3 Low Delay
  
     build\x86_windows\x64\Release\encoder_app --config cfg\encode_LD.cfg -i City_1280x720_60.yuv -w 1280 -h 720 -z 60 -f 9 -d 8 -q 45 -o City_LD.bin -r City_LD_rec.yuv
     build\x86_windows\x64\Release\decoder_app -s -i City_LD.bin -o City_LD_dec.yuv
     
*******************************************************************
4. Configuration files

   The default configuration files are provided in the directory "cfg".
   These contain explanatory comments for each parameter.
  
   If the parameter name is undefined, the program will be terminated with an error message.

*******************************************************************
