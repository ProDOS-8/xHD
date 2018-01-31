AC ?= ac.jar
AS ?= Merlin32

.PHONY: disk

%: %.s
	$(AS) . $<

z8530/xHD: z8530/xHD.s

xHDClient.po:
	cp prodos.po xHDClient.po

disk: z8530/xHD | xHDClient.po
	java -jar $(AC) -d xHDClient.po xHD
	java -jar $(AC) -p xHDClient.po xHD sys < z8530/xHD
