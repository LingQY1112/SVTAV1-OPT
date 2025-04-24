The canonical URL for SVT-AV1 is at <https://gitlab.com/AOMediaCodec/SVT-AV1>

## Build Support 
- __Build Requirements__
  - GCC 5.4.0 or later
  - CMake 3.16 or later
  - NASM Assembler version 2.14 or later
  - Build VMAF support: <https://github.com/Netflix/vmaf>

- __Build and install SVT-AV1__
``` bash
cd SVTAV1-OPT
cd Build
cmake ..
make -j $(nproc)
```

## Encode Command
1.改进的基于VMAF的RDO：specify tune=3 --vmaf-model-path 为安装VMAF库的具体路径
``` bash
./SvtAv1EncApp -i {Seq_origin.yuv} -w {width} -h {height} --tune 3 --preset 3 --fps {fps} --rc 0 --qp {crf} --enable-qm 1 --keyint 256 --hierarchical-levels 5 --enable-stat-report 1 --stat-file {encode.log} --output {compress.ivf} --recon {Seq_reconstructed.yuv} --vmaf-model-path {/vmaf-2.3.1/model/vmaf_v0.6.1.json}
```
2.前处理：用SSIM和PSNR限制锐化强度：specify tune=4  --vmaf-model-path 为安装VMAF库的具体路径
``` bash
./SvtAv1EncApp -i {Seq_origin.yuv} -w {width} -h {height} --tune 4 --preset 3 --fps {fps} --rc 0 --qp {crf} --enable-qm 1 --keyint 256 --hierarchical-levels 5 --enable-stat-report 1 --stat-file {encode.log} --output {compress.ivf} --recon {Seq_reconstructed.yuv} --vmaf-model-path {/vmaf-2.3.1/model/vmaf_v0.6.1.json}
```
### Acknowledgement
This repository is based on <https://aomedia.googlesource.com/aom/+/3cd9eec>.
The reference paper is VMAF Based Rate-Distortion Optimization for Video Coding

