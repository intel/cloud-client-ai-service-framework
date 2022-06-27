#
# Copyright (C) 2020-2021 Intel Corporation
# SPDX-License-Identifier: Apache-2.0
#
# This file is based in part on ctcdecode/__init__.py from https://github.com/parlance/ctcdecode,
# commit 431408f22d93ef5ebc4422995111bbb081b971a9 on Apr 4, 2020, 20:54:49 UTC+1.
#
from types import SimpleNamespace as namespace

import numpy as np

from . import impl

alphabet_tmp = [b'\x01',b'\x02',b'\x03',b'\x04',b'\x05',b'\x06',b'\x07',b'\x08',b'\x09',b'\x0a',b'\x0b',b'\x0c',b'\x0d',b'\x0e',b'\x0f',b'\x10',b'\x11',b'\x12',b'\x13',b'\x14',b'\x15',b'\x16',b'\x17',b'\x18',b'\x19',b'\x1a',b'\x1b',b'\x1c',b'\x1d',b'\x1e',b'\x1f',b'\x20',b'\x21',b'\x22',b'\x23',b'\x24',b'\x25',b'\x26',b'\x27',b'\x28',b'\x29',b'\x2a',b'\x2b',b'\x2c',b'\x2d',b'\x2e',b'\x2f',b'\x30',b'\x31',b'\x32',b'\x33',b'\x34',b'\x35',b'\x36',b'\x37',b'\x38',b'\x39',b'\x3a',b'\x3b',b'\x3c',b'\x3d',b'\x3e',b'\x3f',b'\x40',b'\x41',b'\x42',b'\x43',b'\x44',b'\x45',b'\x46',b'\x47',b'\x48',b'\x49',b'\x4a',b'\x4b',b'\x4c',b'\x4d',b'\x4e',b'\x4f',b'\x50',b'\x51',b'\x52',b'\x53',b'\x54',b'\x55',b'\x56',b'\x57',b'\x58',b'\x59',b'\x5a',b'\x5b',b'\x5c',b'\x5d',b'\x5e',b'\x5f',b'\x60',b'\x61',b'\x62',b'\x63',b'\x64',b'\x65',b'\x66',b'\x67',b'\x68',b'\x69',b'\x6a',b'\x6b',b'\x6c',b'\x6d',b'\x6e',b'\x6f',b'\x70',b'\x71',b'\x72',b'\x73',b'\x74',b'\x75',b'\x76',b'\x77',b'\x78',b'\x79',b'\x7a',b'\x7b',b'\x7c',b'\x7d',b'\x7e',b'\x7f',b'\x80',b'\x81',b'\x82',b'\x83',b'\x84',b'\x85',b'\x86',b'\x87',b'\x88',b'\x89',b'\x8a',b'\x8b',b'\x8c',b'\x8d',b'\x8e',b'\x8f',b'\x90',b'\x91',b'\x92',b'\x93',b'\x94',b'\x95',b'\x96',b'\x97',b'\x98',b'\x99',b'\x9a',b'\x9b',b'\x9c',b'\x9d',b'\x9e',b'\x9f',b'\xa0',b'\xa1',b'\xa2',b'\xa3',b'\xa4',b'\xa5',b'\xa6',b'\xa7',b'\xa8',b'\xa9',b'\xaa',b'\xab',b'\xac',b'\xad',b'\xae',b'\xaf',b'\xb0',b'\xb1',b'\xb2',b'\xb3',b'\xb4',b'\xb5',b'\xb6',b'\xb7',b'\xb8',b'\xb9',b'\xba',b'\xbb',b'\xbc',b'\xbd',b'\xbe',b'\xbf',b'\xc0',b'\xc1',b'\xc2',b'\xc3',b'\xc4',b'\xc5',b'\xc6',b'\xc7',b'\xc8',b'\xc9',b'\xca',b'\xcb',b'\xcc',b'\xcd',b'\xce',b'\xcf',b'\xd0',b'\xd1',b'\xd2',b'\xd3',b'\xd4',b'\xd5',b'\xd6',b'\xd7',b'\xd8',b'\xd9',b'\xda',b'\xdb',b'\xdc',b'\xdd',b'\xde',b'\xdf',b'\xe0',b'\xe1',b'\xe2',b'\xe3',b'\xe4',b'\xe5',b'\xe6',b'\xe7',b'\xe8',b'\xe9',b'\xea',b'\xeb',b'\xec',b'\xed',b'\xee',b'\xef',b'\xf0',b'\xf1',b'\xf2',b'\xf3',b'\xf4',b'\xf5',b'\xf6',b'\xf7',b'\xf8',b'\xf9',b'\xfa',b'\xfb',b'\xfc',b'\xfd',b'\xfe',b'\xff']

class SeqCtcLmDecoder:
    """
    Non-batched sequential CTC + n-gram LM beam search decoder
    """
    def __init__(self, alphabet, beam_size, max_candidates=None,
            cutoff_prob=1.0, cutoff_top_n=300,
            scorer_lm_fname=None, alpha=None, beta=None):
        self.alphabet = alphabet
        self.beam_size = beam_size
        self.max_candidates = max_candidates or 0
        self.cutoff_prob = cutoff_prob
        self.cutoff_top_n = cutoff_top_n

        if scorer_lm_fname is not None:
            assert alpha is not None and beta is not None, "alpha and beta arguments must be provided to use LM"
            self.lm_scorer = impl.ScorerYoklm(alpha, beta, scorer_lm_fname, alphabet)
        else:
            self.lm_scorer = None
        self.decoder_state = impl.CtcDecoderStateNumpy()
        if len(alphabet) != 0:
            self.decoder_state.init(
               alphabet + [''],
               blank_idx = len(alphabet),
               beam_size = beam_size,
               lm_scorer = self.lm_scorer,
            )
        else:    
            self.decoder_state.init(
               alphabet,
               blank_idx = 255,
               beam_size = beam_size,
               lm_scorer = self.lm_scorer,
            )
        self.decoder_state.set_config("cutoff_top_n", cutoff_top_n, required=True)
        self.decoder_state.set_config("cutoff_prob", cutoff_prob, required=True)

    def append_data(self, probs, log_probs):
        """
          Args:
        probs (numpy.ndarray), 2d array with symbol probabilities (frames x symbols)
        log_probs (bool), True to accept natural-logarithmic probabilities, False to accept probabilities directly.
        """
        assert len(probs.shape) == 2
        if self.decoder_state.is_finalized():
            self.decoder_state.new_sequence()
        self.decoder_state.append_numpy(probs, log_probs)

    def decode(self, probs=None, log_probs=None, finalize=True):
        if probs is not None:
            assert log_probs is not None, "When 'probs' argument is provided, 'log_probs' argument must be provided as well"
            self.append_data(probs, log_probs)
        symbols, timesteps, scores, cand_lengths = self.decoder_state.decode_numpy(limit_candidates=self.max_candidates, finalize=finalize)
        cand_starts = np.empty(cand_lengths.shape[0] + 1, dtype=cand_lengths.dtype)
        cand_starts[0] = 0
        cand_lengths.cumsum(out=cand_starts[1:])

        def text_decoder(symbol_idxs):
            if len(self.alphabet) != 0:
                return ''.join(self.alphabet[idx] for idx in symbol_idxs)
            else:
                ch_symbol = b''.join(alphabet_tmp[idx] for idx in symbol_idxs if idx < 256)
                try:
                    ZH = ch_symbol.decode()
                except UnicodeDecodeError:
                    ZH = ''
                return ZH

        candidates = [
            namespace(
                conf=scores[res_idx],
                text=text_decoder(symbols[cand_starts[res_idx]:cand_starts[res_idx+1]]),
                ts=list(timesteps[cand_starts[res_idx]:cand_starts[res_idx+1]]),
            )
            for res_idx in range(cand_lengths.shape[0])
        ]
        return candidates


class BatchedCtcLmDecoder:
    """
    Batched CTC + n-gram LM beam search decoder
    """
    def __init__(self, alphabet, model_path=None, alpha=0, beta=0, cutoff_top_n=300, cutoff_prob=1.0, beam_width=100,
                 max_candidates_per_batch=None, num_processes=4, blank_id=0, log_probs_input=False, loader='yoklm'):
        self.cutoff_top_n = cutoff_top_n
        self._beam_width = beam_width
        self._max_candidates_per_batch = max_candidates_per_batch
        self._scorer = None
        self._num_processes = num_processes
        self._alphabet = list(alphabet)
        self._blank_id = blank_id
        self._log_probs = bool(log_probs_input)
        if model_path is not None:
            if loader == 'yoklm':
                self._scorer = impl.ScorerYoklm(alpha, beta, model_path, self._alphabet)
            else:
                raise ValueError("Unknown loader type: \"%s\"" % loader)
        self._cutoff_prob = cutoff_prob

    def decode(self, probs, seq_lens=None):
        # We expect probs as batch x seq x label_size
        batch_size, max_seq_len = probs.shape[0], probs.shape[1]
        if seq_lens is None:
            seq_lens = np.full(batch_size, max_seq_len, dtype=np.int32)
        max_candidates_per_batch = self._max_candidates_per_batch
        if max_candidates_per_batch is None or max_candidates_per_batch > self._beam_width:
            max_candidates_per_batch = self._beam_width

        output, timesteps, scores, out_seq_len = impl.batched_ctc_lm_decoder(
            probs,  # batch_size x max_seq_lens x vocab_size
            seq_lens,  # batch_size
            self._alphabet,  # list(str)
            self._beam_width,
            max_candidates_per_batch,
            self._num_processes,
            self._cutoff_prob,
            self.cutoff_top_n,
            self._blank_id,
            self._log_probs,
            self._scorer,
        )

        output.shape =      (batch_size, max_candidates_per_batch, -1)
        timesteps.shape =   (batch_size, max_candidates_per_batch, -1)
        scores.shape =      (batch_size, max_candidates_per_batch)
        out_seq_len.shape = (batch_size, max_candidates_per_batch)

        return output, scores, timesteps, out_seq_len

    def character_based(self):
        return self._scorer.is_character_based() if self._scorer else None

    def max_order(self):
        return self._scorer.get_max_order() if self._scorer else None

    def dict_size(self):
        return self._scorer.get_dict_size() if self._scorer else None

    def reset_params(self, alpha, beta):
        if self._scorer is not None:
            self._scorer.reset_params(alpha, beta)
