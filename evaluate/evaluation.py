import torch
from collections import OrderedDict
from evaluate.coco_eval import run_eval
from lib.network.rtpose_vgg import get_model


def main():
    # this path is with respect to the project root
    weight_name = './network/weight/best_pose.pth'
    ckpt = torch.load(weight_name, map_location='cpu')

    if isinstance(ckpt, dict) and 'state_dict' in ckpt:
        state_dict = ckpt['state_dict']
    elif isinstance(ckpt, dict):
        state_dict = ckpt
    else:
        raise TypeError(f'Unexpected checkpoint type: {type(ckpt)}')

    # remove DataParallel prefix "module." if present
    new_state_dict = OrderedDict()
    for k, v in state_dict.items():
        name = k.replace('module.', '', 1) if k.startswith('module.') else k
        new_state_dict[name] = v

    model = get_model(trunk='vgg19')
    model.load_state_dict(new_state_dict, strict=True)
    if torch.cuda.is_available():
        model = model.cuda()
    model.eval()
    model.float()

    run_eval(
        image_dir='/data/coco/images/val2017',
        anno_file='/data/coco/annotations/person_keypoints_val2017.json',
        vis_dir='/data/coco/images/vis_val2017',
        model=model,
        preprocess='vgg'
    )


if __name__ == '__main__':
    with torch.no_grad():
        main()
