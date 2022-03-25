# -*- coding: utf-8 -*-
# /usr/bin/python2
'''
By kyubyong park. kbpark.linguist@gmail.com.
https://www.github.com/kyubyong/tacotron
'''

from __future__ import print_function

from hyperparams import Hyperparams as hp
import tqdm
from data_load import load_data
import tensorflow as tf

from train import Graph
import os
import numpy as np
from tensorflow.python.framework import graph_io
import pickle
import struct

def froze_ie_decode_graph():

    if not os.path.exists(hp.sampledir):
        os.mkdir(hp.sampledir)

    graph = tf.Graph()
    with graph.as_default():

    # Load graph
       g = Graph(mode="ie_decode"); 
       #tf.identity(g.z_hat, name="output")
       tf.identity(g.y_hat, name="decode_out")

    # Load data
       texts = load_data(mode="synthesize")
       y_hat = np.zeros((texts.shape[0], 200, hp.n_mels*hp.r), np.float32)  # hp.n_mels*hp.r

       saver = tf.train.Saver()
    with tf.Session(graph=graph) as sess:
        sess.run(tf.global_variables_initializer())

        saver.restore(sess, tf.train.latest_checkpoint(hp.logdir)); 
        print("Restored!")

        output_node_names = "decode_out"
        print(output_node_names.split(','))
        output_graph_def = tf.graph_util.convert_variables_to_constants(sess, graph.as_graph_def(), output_node_names.split(','))
        graph_io.write_graph(output_graph_def,'./models/pb/', 'infer_ie_decode.pb', as_text=False)

def froze_ie_post_graph():

    if not os.path.exists(hp.sampledir):
        os.mkdir(hp.sampledir)

    graph = tf.Graph()
    with graph.as_default():

    # Load graph
       g = Graph(mode="ie_post"); 
       tf.identity(g.z_hat, name="output")
       #tf.identity(g.y_hat, name="decode_out")

    # Load data
       y_hat = np.zeros((1, 200, hp.n_mels*hp.r), np.float32)  # hp.n_mels*hp.r

       saver = tf.train.Saver()
    with tf.Session(graph=graph) as sess:
        sess.run(tf.global_variables_initializer())

        saver.restore(sess, tf.train.latest_checkpoint(hp.logdir)); 
        print("Restored!")

        output_node_names = "output"
        print(output_node_names.split(','))
        output_graph_def = tf.graph_util.convert_variables_to_constants(sess, graph.as_graph_def(), output_node_names.split(','))
        graph_io.write_graph(output_graph_def,'./models/pb/', 'infer_ie_post.pb', as_text=False)

if __name__ == '__main__':
 
    froze_ie_decode_graph()
    froze_ie_post_graph()

    print("Done")

