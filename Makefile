PROJECT=Gallery

TARGETS= model

PROPAGATE= \
for rule in $(TARGETS) ; do \
  make PROJECT=$(PROJECT) DESTDIR=$(DESTDIR) MODULE=$$rule -f Rules.$$rule $@ || exit $$?;\
done


all:	thumbnailer webmize
	@+$(PROPAGATE)

install: thumbnailer
	@+$(PROPAGATE)
	$(SU0) install -d $(DESTDIR)/usr/include/$(PROJECT)/
	$(SU0) install -m 644 *.H $(DESTDIR)/usr/include/$(PROJECT)/
	$(SU0) install -d $(DESTDIR)/usr/lib/gallery
	$(SU0) install -m 755 thumbnailer $(DESTDIR)/usr/lib/gallery/

clean:
	rm -f thumbnailer webmize
	@+$(PROPAGATE)

thumbnailer: thumbnailer.C
	`RaiiBuild` -I/usr/include/ImageMagick-6 -I/usr/include/x86_64-linux-gnu/ImageMagick-6/ -DMAGICKCORE_HDRI_ENABLE=0 -DMAGICKCORE_QUANTUM_DEPTH=16 thumbnailer.C -o thumbnailer -lMagick++

webmize:
	cc -Werror -Wall -ggdb3 webmize.c -o webmize
