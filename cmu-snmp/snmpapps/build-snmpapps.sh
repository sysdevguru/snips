#!/bin/sh

(smake wash mf all install 2>&1) | tee log.`sys`
exit 0
