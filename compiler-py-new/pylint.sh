#!/bin/sh
pylint --disable-msg-cat=ICR --reports=no $* | grep -v 'Unused import' | grep -v 'Unused argument' | grep -v ':foo: function already defined' | grep -v 'but some types could not be inferred' | grep -v 'Wildcard import' | grep -v 'Using the global statement'
