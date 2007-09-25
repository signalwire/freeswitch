package org.freeswitch;

public interface DTMFCallback
{
    String onDTMF(Object input, int inputType, String args);
}

