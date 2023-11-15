ROOT_DIR := $(shell dirname $(realpath $(firstword $(MAKEFILE_LIST))))

INFO = $(wildcard *.info)
SAMPLE = $(wildcard *.sample)
GL = $(wildcard *.gl)

SCAT = $(patsubst %.sample,scat_%.png,$(SAMPLE))
HIST = $(patsubst %.sample,hist_%.png,$(SAMPLE))
TIME = $(patsubst %.sample,time_%.png,$(SAMPLE))
EXEC = $(patsubst %.sample,exec_%.png,$(SAMPLE))

COUNT = $(patsubst %.info,%.count,$(INFO))

GLF = $(patsubst %.gl,%.glf,$(GL))

all: $(SCAT) $(HIST) $(TIME) $(EXEC) $(COUNT) $(GLF)

scat_%.png: %.info %.sample
	python3 $(ROOT_DIR)/scat.py $*

hist_%.png: %.info %.sample
	python3 $(ROOT_DIR)/hist.py $*

time_%.png: %.info %.sample
	python3 $(ROOT_DIR)/time.py $*

exec_%.png: %.info
	python3 $(ROOT_DIR)/exec.py $*

%.count: %.info
	python3 $(ROOT_DIR)/count.py $* > $@

%.glf: %.gl
	python3 $(ROOT_DIR)/filter_gl.py $* > $@
