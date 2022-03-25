import numpy as np


class CTCCodec(object):
    """ Convert index to label """
    def __init__(self, char_label, top_k):
        # char_label : all the characters.

        self.top_k = top_k
        self.index = {}

        list_character = list(char_label)
        for i, char in enumerate(list_character):
            # 0 is for 'blank'
            self.index[char] = i + 1

        self.char_label = ['[blank]'] + list_character

    def decode(self, predicts):
        """ convert index to label. """
        texts_label = []
        text_list = []

        # Select max probability
        index_predicts = np.argmax(predicts, 2) # WBD - > WB
        index_predicts = index_predicts.transpose(1, 0) # WB -> BW
        index_predicts_reshape = index_predicts.reshape(-1) # B*W

        for i in range(len(index_predicts_reshape)):
            if index_predicts_reshape[i] != 0 and (not (i > 0 and index_predicts_reshape[i] == index_predicts_reshape[i - 1])):
                text_list.append(self.char_label[index_predicts_reshape[i]])

        text = ''.join(text_list)
        texts_label.append(text)

        return texts_label
