#!/bin/sh
pylint --disable-msg-cat=ICR --reports=no $* | grep -v 'Unused import'
