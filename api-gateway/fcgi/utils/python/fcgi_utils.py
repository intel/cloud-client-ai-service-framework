# -*- coding: utf-8 -*-
# /usr/bin/python3

import numpy as np
import librosa
import copy
from scipy import signal
import codecs
import re
import os
import unicodedata

class Hyperparams:

    vocab = "PE abcdefghijklmnopqrstuvwxyz'.?" # P: Padding E: End of Sentence

    # data
    test_data = 'harvard_sentences.txt'
    max_duration = 10.0

    # signal processing
    sr = 22050 # Sample rate.
    n_fft = 2048 # fft points (samples)
    frame_shift = 0.0125 # seconds
    frame_length = 0.05 #0.05 # seconds
    hop_length = int(sr * frame_shift) # samples. 275.6, 200
    win_length = int(sr * frame_length) # samples. 1102, 800
    n_mels = 80 # Number of Mel banks to generate
    power = 1.2 # Exponent for amplifying the predicted magnitude
    n_iter = 50 # Number of inversion iterations
    preemphasis = .97 # or None
    max_db = 100
    ref_db = 20

    # model
    r = 5 # Reduction factor. Paper => 2, 3, 5

hp = Hyperparams()

def spectrogram2wav(mag):
    '''# Generate wave file from spectrogram'''
    # transpose
    mag = mag.T

    # de-noramlize
    mag = (np.clip(mag, 0, 1) * hp.max_db) - hp.max_db + hp.ref_db

    # to amplitude
    mag = np.power(10.0, mag * 0.05)

    # wav reconstruction
    wav = griffin_lim(mag)

    # de-preemphasis
    wav = signal.lfilter([1], [1, -hp.preemphasis], wav)

    # trim
    wav, _ = librosa.effects.trim(wav)

    return wav.astype(np.float32)

#librosa.stft
#Returns
#    -------
#    D : np.ndarray [shape=(1 + n_fft/2, t), dtype=dtype]
#        STFT matrix

def griffin_lim(spectrogram):

    X_best = copy.deepcopy(spectrogram)
    for i in range(hp.n_iter):
        X_t = invert_spectrogram(X_best)
        est = librosa.stft(X_t, hp.n_fft, hp.hop_length, win_length = hp.win_length)
        phase = est / np.maximum(1e-8, np.abs(est))
        X_best = spectrogram * phase

    X_t = invert_spectrogram(X_best)
    y = np.real(X_t)

    return y


def invert_spectrogram(spectrogram):

    return librosa.istft(spectrogram, hp.hop_length, win_length = hp.win_length, window = "hann")

def load_vocab():
    char2idx = {char: idx for idx, char in enumerate(hp.vocab)}
    idx2char = {idx: char for idx, char in enumerate(hp.vocab)}
    return char2idx, idx2char

def text_normalize(text):
    text = ''.join(char for char in unicodedata.normalize('NFD', text) # unicode ---> NFD format
                           if unicodedata.category(char) != 'Mn') # Strip accents

    text = text.lower()
    text = re.sub("[^{}]".format(hp.vocab), " ", text)
    text = re.sub("[ ]+", " ", text)
    return text

def text_to_vector(lines):
    # Load vocabulary
    char2idx, idx2char = load_vocab()

    # Parse
    lines_change = [text_normalize(line.strip()).strip() for line in lines]
    sents = []
    for line in lines_change:
        segment = len(line) // 42
        remain = len(line) % 42

        for i in range(segment):
            sents.append(line[(42 * i):(42 * (i + 1))] + "E")# text normalization, E: EOS
        if remain != 0:
            sents.append(line[-remain:] + "E")

    lengths = [len(sent) for sent in sents]
    maxlen = 43
    texts = np.zeros((len(sents), maxlen), np.float32)

    for i, sent in enumerate(sents):
        texts[i, :len(sent)] = [char2idx[char] for char in sent]

    return texts
