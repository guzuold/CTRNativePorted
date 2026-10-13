# SPDX-License-Identifier: GPL-2.0-only
"""Audit pinned public null-image policy; no guest payload or GPU admission."""
from pathlib import Path
import hashlib, json
root = Path(__file__).resolve().parents[1]
paths = {
 'rasterizer':'engine/shadPS4/src/video_core/renderer_vulkan/vk_rasterizer.cpp',
 'resource':'engine/shadPS4/src/video_core/amdgpu/resource.h',
 'format':'engine/shadPS4/src/video_core/amdgpu/pixel_format.h',
 'device_reference':'engine/shadPS4/src/video_core/renderer_vulkan/vk_instance.cpp',
 'device_selected':'ctrport/video/device_control.cpp',
 'decoder':'ctrport/video/graphics_companion.cpp',
}
s = {k:(root/p).read_text(encoding='utf-8') for k,p in paths.items()}
checks = {
 'public_null_predicate': 'tsharp.Address() == 0 || tsharp.GetDataFmt() == AmdGpu::DataFormat::FormatInvalid' in s['rasterizer'],
 'public_null_descriptor': 'image_infos.emplace_back(VK_NULL_HANDLE, VK_NULL_HANDLE, vk::ImageLayout::eGeneral)' in s['rasterizer'],
 'public_null_array_one': 'image_descriptor_array_sizes.push_back(1);' in s['rasterizer'],
 'type_validity_is_distinct': 'return (type & 0x8u) != 0;' in s['resource'],
 'invalid_format_zero': 'FormatInvalid = 0,' in s['format'],
 'public_feature_enabled': '.nullDescriptor = true,' in s['device_reference'],
 'selected_feature_queried_enabled': 'enabledRobust.nullDescriptor=availableRobust.nullDescriptor;' in s['device_selected'] and 'VK_EXT_ROBUSTNESS_2_EXTENSION_NAME' in s['device_selected'],
 'decoder_rejects_before_parameters': 'if(!image.Valid())return 88;' in s['decoder'],
}
assert all(checks.values()), checks
report = {
 'scope':'Static pinned-public-source policy audit; no retail descriptor classification or GPU null write executed.',
 'checks':checks,
 'sources':{k:{'path':p,'sha256':hashlib.sha256((root/p).read_bytes()).hexdigest()} for k,p in paths.items()},
 'policy':{
  'null':'Address()==0 OR remapped data format==FormatInvalid; evaluated before normal texture lookup.',
  'invalid_type':'Image.Valid()==false alone does not establish public null semantics.',
  'descriptor':'Public renderer writes null imageView with GENERAL layout; no source read or invented texture.',
  'feature':'Reference explicitly enables robustness2 nullDescriptor. Selected device now queries extension and nullDescriptor, enabling only when available. Descriptor writes still unimplemented.',
  'array':'Public null path contributes one descriptor; paired count>1 needs explicit reconciliation, not repeated fabricated resources.',
 },
 'retail_word4911_sharp112_classified':False,
 'missing':['Typed null predicate from original retained 8DWORD sharp','Owned null descriptor update with paired layout and GPU completion lifetime'],
 'runtime_R3':False,'graphics_replacements':0,
}
(root/'reports/reconstruction/data/null_image_binding_contract.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
print('null image public policy: 8 assertions passed; descriptor writes remain unimplemented')
