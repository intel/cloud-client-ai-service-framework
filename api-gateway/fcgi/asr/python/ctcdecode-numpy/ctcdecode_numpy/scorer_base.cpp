/*********************************************************************
* Copyright (c) 2020-2021 Intel Corporation
* SPDX-License-Identifier: Apache-2.0
*
* This file is based in part on scorer.cpp from https://github.com/parlance/ctcdecode,
* commit 431408f22d93ef5ebc4422995111bbb081b971a9 on Apr 4, 2020, 20:54:49 UTC+1.
**********************************************************************/

#include "scorer_base.h"

#include <cassert>

#include "decoder_utils.h"

static std::vector<std::string> vocab_utf8;

ScorerBase::ScorerBase(double alpha,
                       double beta)
      : alpha(alpha), beta(beta), dictionary(nullptr),
        is_character_based_(true), max_order_(0),
        dict_size_(0), space_id_(-1) {
  // Derived classes must call setup() from derived class constructor
  // since setup() calls virtual method load_lm():
}

ScorerBase::~ScorerBase() {}

void ScorerBase::setup(const std::string& lm_path,
                       const std::vector<std::string>& vocab_list) {
  // load language model
  load_lm(lm_path);
  // set char map for scorer
  if (vocab_list.empty()) {
     vocab_utf8.clear();
     for (size_t i = 0; i < 255; ++i) {
        std::string val(1, i+1);
        vocab_utf8.push_back(val);//vocab: label_to_string(index-->value):(int, string)
    }
    set_char_map(vocab_utf8);
  } else
    set_char_map(vocab_list);
  // fill word prefix dictionary
  bool add_space = true;
  if (is_character_based())
    add_space = false; 
  fill_dictionary(add_space);
}

double ScorerBase::get_sent_log_prob(const std::vector<std::string>& words) {
  std::vector<std::string> sentence;
  if (words.size() == 0) {
    for (size_t i = 0; i < max_order_; ++i) {
      sentence.push_back(START_TOKEN);
    }
  } else {
    for (size_t i = 0; i < max_order_ - 1; ++i) {
      sentence.push_back(START_TOKEN);
    }
    sentence.insert(sentence.end(), words.begin(), words.end());
  }
  sentence.push_back(END_TOKEN);
  return get_log_prob(sentence);
}

double ScorerBase::get_log_prob(const std::vector<std::string>& words) {
  assert(words.size() > max_order_);
  double score = 0.0;
  for (size_t i = 0; i < words.size() - max_order_ + 1; ++i) {
    std::vector<std::string> ngram(words.begin() + i,
                                   words.begin() + i + max_order_);
    score += get_log_cond_prob(ngram);
  }
  return score;
}

void ScorerBase::reset_params(float alpha, float beta) {
  this->alpha = alpha;
  this->beta = beta;
}

std::string ScorerBase::vec2str(const std::vector<int>& input) {
  std::string word;
  for (auto ind : input) {
    word += char_list_[ind];
  }
  return word;
}

std::vector<std::string> ScorerBase::split_labels(const std::vector<int>& labels) {
  if (labels.empty()) return {};

  std::string s = vec2str(labels);
  std::vector<std::string> words;
  if (is_character_based_) {
    words = split_utf8_str(s);
  } else {
    words = split_str(s, " ");
  }
  return words;
}

void ScorerBase::set_char_map(const std::vector<std::string>& char_list) {
  char_list_ = char_list;
  char_map_.clear();

  if (char_list_.size() == 255)
    space_id_ = ' ' - 1;

  for (size_t i = 0; i < char_list_.size(); i++) {
    if ((char_list_[i] == " ") && (char_list_.size() != 255)) {
      space_id_ = i;
    }
    // The original implementation avoided 0, we keep this behavior for now for simplicity:
    //   "The initial state of FST is state 0, hence the index of chars in
    //   the FST should start from 1 to avoid the conflict with the initial
    //   state, otherwise wrong decoding results would be given."
    char_map_[char_list_[i]] = i + 1;
  }
}

std::vector<std::string> ScorerBase::make_ngram(PathTrie* prefix) {
  std::vector<std::string> ngram;
  PathTrie* current_node = prefix;
  PathTrie* new_node = nullptr;

  for (size_t order = 0; order < max_order_; order++) {
    std::vector<int> prefix_vec;
    std::vector<int> prefix_steps;

    if (!current_node || current_node->character == -1) {
      for (size_t i = 0; i < max_order_ - order - 1; i++) {
        ngram.push_back(START_TOKEN);
      }
      break;
    }

    if (is_character_based_) {
      new_node = current_node->get_prev_grapheme(prefix_vec, prefix_steps, char_list_);
    } else {
      new_node = current_node->get_path_vec(prefix_vec, prefix_steps, space_id_);
    }
    current_node = new_node->parent;

    // reconstruct word
    std::string word = vec2str(prefix_vec);
    ngram.push_back(word);

    if (new_node->character == -1) {
      // No more spaces, but still need order
      for (size_t i = 0; i < max_order_ - order - 1; i++) {
        ngram.push_back(START_TOKEN);
      }
      break;
    }
  }
  std::reverse(ngram.begin(), ngram.end());
  return ngram;
}

void ScorerBase::fill_dictionary(bool add_space) {
  // For each unigram convert to ints and store
  std::vector<std::vector<int> > int_vocabulary;
  for (const auto& word : vocabulary_) {
      add_word_to_dictionary(word, char_map_, add_space, space_id_ + 1, int_vocabulary);
  } //utf-8 not inset space at the end of word. aphabet mode insert space at the end of word

  // Add the converted vocabulary to WordPrefixSet
  this->dictionary.reset(new WordPrefixSet);
  dict_size_ = this->dictionary->add_words(int_vocabulary);
}

int ScorerBase::distance_to_codepoint_boundary(PathTrie* current, unsigned char *first_byte)
{
  if (byte_is_codepoint_boundary(char_list_[current->character][0])) {
    *first_byte = (unsigned char)(current->character) + 1;
    return 1;
  }
  if (current->parent != nullptr && !current->parent->is_empty()) {
    return 1 + distance_to_codepoint_boundary(current->parent, first_byte);
  }
  return 0;

}

bool ScorerBase::is_scoring_boundary(PathTrie* prefix, int new_label)
{
  if (is_character_based_) {
    if (prefix->character == -1) {
      return false;
    }
    unsigned char first_byte;
    int distance_to_boundary = distance_to_codepoint_boundary(prefix, &first_byte);
    int needed_bytes;
    if ((first_byte >> 3) == 0x1E) {
      needed_bytes = 4;
    } else if ((first_byte >> 4) == 0x0E) {
      needed_bytes = 3;
    } else if ((first_byte >> 5) == 0x06) {
      needed_bytes = 2;
    } else if ((first_byte >> 7) == 0x00) {
      needed_bytes = 1;
    } else {
      assert(false); // invalid byte sequence. should be unreachable, disallowed by vocabulary/trie
      return false;
    }

    return distance_to_boundary == needed_bytes;
  } else {
    return new_label == space_id_;
  }
}

