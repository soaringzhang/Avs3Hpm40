./app_encoder --config ../cfg/encode_IPPP.cfg -v 2 -i akiyo_qcif.yuv -w 176 -h 144 -z 30 -o akiyo_qcif.enc -r akiyo_qcif.rec
diff akiyo_qcif.enc akiyo_qcif.enc.ref && echo "Success!"
