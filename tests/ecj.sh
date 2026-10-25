#!/bin/sh
# Run Ubuntu's Eclipse compiler on a current JDK even while testing a Java 8 VM.
set -eu
exec "${JPP_ECJ_JAVA:-java}" -cp /usr/share/java/eclipse-jdt-core.jar \
    org.eclipse.jdt.internal.compiler.batch.Main "$@"
