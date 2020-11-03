from typing import List, Dict, Any
import torch
import trtorch._C
from trtorch import _types


def _supported_input_size_type(input_size: Any) -> bool:
    if isinstance(input_size, torch.Size):
        return True
    elif isinstance(input_size, tuple):
        return True
    elif isinstance(input_size, list):
        return True
    else:
        raise TypeError(
            "Input sizes for inputs are required to be a List, tuple or torch.Size or a Dict of three sizes (min, opt, max), found type: "
            + str(type(input_size)))


def _parse_input_ranges(input_sizes: List) -> List:

    if any(not isinstance(i, dict) and not _supported_input_size_type(i) for i in input_sizes):
        raise KeyError("An input size must either be a static size or a range of three sizes (min, opt, max) as Dict")

    parsed_input_sizes = []
    for i in input_sizes:
        if isinstance(i, dict):
            if all(k in i for k in ["min", "opt", "min"]):
                in_range = trtorch._C.InputRange()
                in_range.min = i["min"]
                in_range.opt = i["opt"]
                in_range.max = i["max"]
                parsed_input_sizes.append(in_range)

            elif "opt" in i:
                in_range = trtorch._C.InputRange()
                in_range.min = i["opt"]
                in_range.opt = i["opt"]
                in_range.max = i["opt"]
                parsed_input_sizes.append(in_range)

            else:
                raise KeyError(
                    "An input size must either be a static size or a range of three sizes (min, opt, max) as Dict")

        elif isinstance(i, list):
            in_range = trtorch._C.InputRange()
            in_range.min = i
            in_range.opt = i
            in_range.max = i
            parsed_input_sizes.append(in_range)

        elif isinstance(i, tuple):
            in_range = trtorch._C.InputRange()
            in_range.min = list(i)
            in_range.opt = list(i)
            in_range.max = list(i)
            parsed_input_sizes.append(in_range)

    return parsed_input_sizes


def _parse_op_precision(precision: Any) -> _types.dtype:
    if isinstance(precision, torch.dtype):
        if precision == torch.int8:
            return _types.dtype.int8
        elif precision == torch.half:
            return _types.dtype.half
        elif precision == torch.float:
            return _types.dtype.float
        else:
            raise TypeError("Provided an unsupported dtype as operating precision (support: int8, half, float), got: " +
                            str(precision))

    elif isinstance(precision, _types.DataTypes):
        return precision

    else:
        raise TypeError("Op precision type needs to be specified with a torch.dtype or a trtorch.dtype, got: " +
                        str(type(precision)))


def _parse_device_type(device: Any) -> _types.DeviceType:
    if isinstance(device, torch.device):
        if device.type == 'cuda':
            return _types.DeviceType.gpu
        else:
            ValueError("Got a device type other than GPU or DLA (type: " + str(device.type) + ")")
    elif isinstance(device, _types.DeviceType):
        return device
    elif isinstance(device, str):
        if device == "gpu" or device == "GPU":
            return _types.DeviceType.gpu
        elif device == "dla" or device == "DLA":
            return _types.DeviceType.dla
        else:
            ValueError("Got a device type other than GPU or DLA (type: " + str(device) + ")")
    else:
        raise TypeError("Device specification must be of type torch.device, string or trtorch.DeviceType, but got: " +
                        str(type(device)))


def _parse_compile_spec(compile_spec: Dict[str, Any]) -> trtorch._C.CompileSpec:
    info = trtorch._C.CompileSpec()
    if "input_shapes" not in compile_spec:
        raise KeyError(
            "Input shapes for inputs are required as a List, provided as either a static sizes or a range of three sizes (min, opt, max) as Dict"
        )

    info.input_ranges = _parse_input_ranges(compile_spec["input_shapes"])

    if "op_precision" in compile_spec:
        info.op_precision = _parse_op_precision(compile_spec["op_precision"])

    if "refit" in compile_spec:
        assert isinstance(compile_spec["refit"], bool)
        info.refit = compile_spec["refit"]

    if "debug" in compile_spec:
        assert isinstance(compile_spec["debug"], bool)
        info.debug = compile_spec["debug"]

    if "strict_types" in compile_spec:
        assert isinstance(compile_spec["strict_types"], bool)
        info.strict_types = compile_spec["strict_types"]

    if "allow_gpu_fallback" in compile_spec:
        assert isinstance(compile_spec["allow_gpu_fallback"], bool)
        info.allow_gpu_fallback = compile_spec["allow_gpu_fallback"]

    if "device_type" in compile_spec:
        info.device = _parse_device_type(compile_spec["device_type"])

    if "capability" in compile_spec:
        assert isinstance(compile_spec["capability"], _types.EngineCapability)
        info.capability = compile_spec["capability"]

    if "num_min_timing_iters" in compile_spec:
        assert type(compile_spec["num_min_timing_iters"]) is int
        info.num_min_timing_iters = compile_spec["num_min_timing_iters"]

    if "num_avg_timing_iters" in compile_spec:
        assert type(compile_spec["num_avg_timing_iters"]) is int
        info.num_avg_timing_iters = compile_spec["num_avg_timing_iters"]

    if "workspace_size" in compile_spec:
        assert type(compile_spec["workspace_size"]) is int
        info.workspace_size = compile_spec["workspace_size"]

    if "max_batch_size" in compile_spec:
        assert type(compile_spec["max_batch_size"]) is int
        info.max_batch_size = compile_spec["max_batch_size"]

    return info


def TensorRTCompileSpec(compile_spec: Dict[str, Any]):
    """
    Utility to create a formated spec dictionary for using the PyTorch TensorRT backend

    Args:
        compile_spec (dict): Compilation settings including operating precision, target device, etc.
            One key is required which is ``input_shapes``, describing the input sizes or ranges for inputs
            to the graph. All other keys are optional. Entries for each method to be compiled.

            .. code-block:: py

                CompileSpec = {
                    "forward" : trtorch.TensorRTCompileSpec({
                        "input_shapes": [
                            (1, 3, 224, 224), # Static input shape for input #1
                            {
                                "min": (1, 3, 224, 224),
                                "opt": (1, 3, 512, 512),
                                "max": (1, 3, 1024, 1024)
                            } # Dynamic input shape for input #2
                        ],
                        "op_precision": torch.half, # Operating precision set to FP16
                        "refit": False, # enable refit
                        "debug": False, # enable debuggable engine
                        "strict_types": False, # kernels should strictly run in operating precision
                        "allow_gpu_fallback": True, # (DLA only) Allow layers unsupported on DLA to run on GPU
                        "device": torch.device("cuda"), # Type of device to run engine on (for DLA use trtorch.DeviceType.DLA)
                        "capability": trtorch.EngineCapability.DEFAULT, # Restrict kernel selection to safe gpu kernels or safe dla kernels
                        "num_min_timing_iters": 2, # Number of minimization timing iterations used to select kernels
                        "num_avg_timing_iters": 1, # Number of averaging timing iterations used to select kernels
                        "workspace_size": 0, # Maximum size of workspace given to TensorRT
                        "max_batch_size": 0, # Maximum batch size (must be >= 1 to be set, 0 means not set)
                    })
                }

            Input Sizes can be specified as torch sizes, tuples or lists. Op precisions can be specified using
            torch datatypes or trtorch datatypes and you can use either torch devices or the trtorch device type enum
            to select device type.

    Returns:
        torch.classes.tensorrt.CompileSpec: List of methods and formated spec objects to be provided to ``torch._C._jit_to_tensorrt``
    """

    parsed_spec = _parse_compile_spec(compile_spec)

    backend_spec = torch.classes.tensorrt.CompileSpec()

    for i in parsed_spec.input_ranges:
        ir = torch.classes.tensorrt.InputRange()
        ir.set_min(i.min)
        ir.set_opt(i.opt)
        ir.set_max(i.max)
        backend_spec.append_input_range(ir)

    backend_spec.set_op_precision(int(parsed_spec.op_precision))
    backend_spec.set_refit(parsed_spec.refit)
    backend_spec.set_debug(parsed_spec.debug)
    backend_spec.set_refit(parsed_spec.refit)
    backend_spec.set_strict_types(parsed_spec.strict_types)
    backend_spec.set_allow_gpu_fallback(parsed_spec.allow_gpu_fallback)
    backend_spec.set_device(int(parsed_spec.device))
    backend_spec.set_capability(int(parsed_spec.capability))
    backend_spec.set_num_min_timing_iters(parsed_spec.num_min_timing_iters)
    backend_spec.set_num_avg_timing_iters(parsed_spec.num_avg_timing_iters)
    backend_spec.set_workspace_size(parsed_spec.workspace_size)
    backend_spec.set_max_batch_size(parsed_spec.max_batch_size)

    return backend_spec
