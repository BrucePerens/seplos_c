all: commands/seplos/seplos

library/libseplos.a: .PHONY
	(cd library; make);

commands/seplos/seplos: library/libseplos.a
	(cd commands/seplos; make)

.PHONY:
