#!/usr/bin/env python3

import torch
import torchvision

m = torchvision.models.resnet18(pretrained=True)
m.eval()
traced_module = torch.jit.trace(m, torch.rand(1, 3, 224, 224))
traced_module.save("resnet18.pt")
