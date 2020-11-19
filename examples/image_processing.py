# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT license.

from eva import EvaProgram, Input, Output, evaluate
from eva.ckks import CKKSCompiler
from eva.seal import generate_keys
from eva.metric import valuation_mse
from PIL import Image
import numpy as np

def convolution(image, width, filter):
    for i in range(len(filter)):
        for j in range(len(filter[0])):
            rotated = image << i * width + j
            partial = rotated * filter[i][j]
            if i == 0 and j == 0:
                convolved = partial
            else:
                convolved += partial
    return convolved

def convolutionXY(image, width, filter):
    for i in range(len(filter)):
        for j in range(len(filter[0])):
            rotated = image << (i * width + j)
            horizontal = rotated * filter[i][j]
            vertical = rotated * filter[j][i]
            if i == 0 and j == 0:
                Ix = horizontal
                Iy = vertical
            else:
                Ix += horizontal
                Iy += vertical
    return Ix, Iy

h = 64
w = 64

sobel = EvaProgram('sobel', vec_size=h*w)
with sobel:
    image = Input('image')

    sobel_filter = [
        [-1, 0, 1],
        [-2, 0, 2],
        [-1, 0, 1]]

    a1 = 2.2137874823876622
    a2 = -1.0984324107372518
    a3 = 0.17254603006834726

    conv_hor, conv_ver = convolutionXY(image, w, sobel_filter)

    conv_hor2 = conv_hor**2
    conv_ver2 = conv_ver**2
    dsq = conv_hor2 + conv_ver2
    dsq2 = dsq * dsq
    dsq3 = dsq2 * dsq

    Output('image', dsq * a1 + dsq2 * a2 + dsq3 * a3)

sobel.set_input_scales(25)
sobel.set_output_ranges(10)

harris = EvaProgram('harris', vec_size=h*w)
with harris:
    image = Input('image')

    sobel_filter = [
        [-1, 0, 1],
        [-2, 0, 2],
        [-1, 0, 1]]
    pool = [
        [1, 1, 1],
        [1, 1, 1],
        [1, 1, 1]]

    c = 0.04

    Ix, Iy = convolutionXY(image, w, sobel_filter)
    
    Ixx = Ix**2
    Iyy = Iy**2
    Ixy = Ix * Iy

    #FIX: masking may be needed here (to handle boundaries)

    Sxx = convolution(Ixx, w, pool)
    Syy = convolution(Iyy, w, pool)
    Sxy = convolution(Ixy, w, pool)

    SxxSyy = Sxx * Syy
    SxySxy = Sxy * Sxy
    det = SxxSyy - SxySxy
    trace = Sxx + Syy

    Output('image', det - trace**2 * c)

harris.set_input_scales(30)
harris.set_output_ranges(20)

def read_input_image():
    image = Image.open('baboon.png').convert('L')
    image_data = [x / 255.0 for x in list(image.getdata())]
    return {'image': image_data}

def write_output_image(outputs, tag):
    enc_result_image = Image.new('L', (w, h))
    enc_result_image.putdata([x * 255.0 for x in outputs['image'][0:h*w]])
    enc_result_image.save(f'baboon_{tag}.png', "PNG")

if __name__ == "__main__":
    inputs = read_input_image()

    for prog in [sobel, harris]:
        print(f'Compiling {prog.name}')

        compiler = CKKSCompiler()
        compiled, params, signature = compiler.compile(prog)
        public_ctx, secret_ctx = generate_keys(params)
        enc_inputs = public_ctx.encrypt(inputs, signature)
        enc_outputs = public_ctx.execute(compiled, enc_inputs)
        outputs = secret_ctx.decrypt(enc_outputs, signature)

        write_output_image(outputs, f'{compiled.name}_encrypted')

        reference = evaluate(compiled, inputs)
        write_output_image(reference, f'{compiled.name}_reference')
        
        print('MSE', valuation_mse(outputs, reference))
        print()
