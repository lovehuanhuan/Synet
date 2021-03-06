/*
* Synet Framework (http://github.com/ermig1979/Synet).
*
* Copyright (c) 2018-2018 Yermalayeu Ihar.
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/

#pragma once

#include "Synet/BatchNormLayer.h"
#include "Synet/BiasLayer.h"
#include "Synet/CastLayer.h"
#include "Synet/ConcatLayer.h"
#include "Synet/ConstLayer.h"
#include "Synet/ConvolutionLayer.h"
#include "Synet/DetectionOutputLayer.h"
#include "Synet/EltwiseLayer.h"
#include "Synet/ExpandDimsLayer.h"
#include "Synet/FillLayer.h"
#include "Synet/FlattenLayer.h"
#include "Synet/GatherLayer.h"
#include "Synet/InnerProductLayer.h"
#include "Synet/InputLayer.h"
#include "Synet/InterpLayer.h"
#include "Synet/LogLayer.h"
#include "Synet/LrnLayer.h"
#include "Synet/MetaLayer.h"
#include "Synet/NormalizeLayer.h"
#include "Synet/PadLayer.h"
#include "Synet/PermuteLayer.h"
#include "Synet/PoolingLayer.h"
#include "Synet/PriorBoxLayer.h"
#include "Synet/RegionLayer.h"
#include "Synet/ReluLayer.h"
#include "Synet/ReorgLayer.h"
#include "Synet/ReshapeLayer.h"
#include "Synet/RestrictRangeLayer.h"
#include "Synet/ScaleLayer.h"
#include "Synet/ShortcutLayer.h"
#include "Synet/SigmoidLayer.h"
#include "Synet/SliceLayer.h"
#include "Synet/SoftmaxLayer.h"
#include "Synet/SqueezeLayer.h"
#include "Synet/StubLayer.h"
#include "Synet/SwitchLayer.h"
#include "Synet/UnaryOperationLayer.h"
#include "Synet/UpsampleLayer.h"
#include "Synet/UnpackLayer.h"
#include "Synet/YoloLayer.h"

namespace Synet
{
    template <class T> class Network
    {
    public:
        typedef T Type;
        typedef Synet::Tensor<T> Tensor;
        typedef std::vector<Tensor*> TensorPtrs;
        typedef Synet::Layer<T> Layer;
        typedef Layer * LayerPtr;
        typedef std::vector<LayerPtr> LayerPtrs;
        typedef Synet::Region<T> Region;
        typedef std::vector<Region> Regions;

        Network()
            : _empty(true)
        {
        }

        bool Empty() const 
        { 
            return _empty; 
        }

        const NetworkParam & Param() const 
        { 
            return _param(); 
        }

        bool Load(const String & param, const String & weight)
        {
            if (!_param.Load(param))
                return false;

            _layers.clear();
            for (size_t i = 0; i < _param().layers().size(); ++i)
            {
                LayerSharedPtr layer(Create(_param().layers()[i]));
                if (layer)
                    _layers.push_back(layer);
            }

            std::ifstream ifs(weight.c_str(), std::ifstream::binary);
            if (!ifs.is_open())
                return false;
            for (size_t i = 0; i < _layers.size(); ++i)
            {
                if (!_layers[i]->Load(ifs))
                {
                    ifs.close();
                    return false;
                }
            }
            ifs.close();

            return Init();
        }

        TensorPtrs & Src() 
        { 
            return _src; 
        }

        const TensorPtrs & Dst() const 
        { 
            return _dst; 
        }

        LayerPtrs Back() const
        {
            return _back;
        }

        bool Reshape(const Strings & srcNames = Strings(), const Shapes & srcShapes = Shapes(), const Strings & dstNames = Strings())
        {
            if (srcNames.size() != srcShapes.size())
                return false;

            if (srcNames.size())
            {
                _src.clear();
                for (size_t i = 0; i < srcNames.size(); ++i)
                {
                    bool found = false;
                    for (size_t j = 0; j < _input.size(); ++j)
                    {
                        const LayerParam & param = _input[j].layer->Param();
                        if (param.name() == srcNames[i])
                        {
                            if (param.type() == LayerTypeInput)
                            {
                                _input[j].dst[0]->Reshape(srcShapes[i]);
                                _src.push_back(_input[j].dst[0]);
                            }
                            else if (param.type() == LayerTypeMeta && param.meta().type() == MetaTypeInput)
                            {
                                Synet::Tensor<int32_t> & i32 = _input[j].dst[0]->As32i();
                                i32.Reshape({ srcShapes[j].size()});
                                for (size_t l = 0; l < srcShapes[j].size(); ++l)
                                    i32.CpuData()[l] = (int)srcShapes[j][l];
                            }
                            else
                                assert(0);
                            found = true;
                            break;
                        }
                    }
                    if (!found)
                        return false;
                }            
            }
            else
            {
                for (size_t i = 0; i < _input.size(); ++i)
                {
                    _input[i].layer->Setup(_input[i].src, _input[i].buf, _input[i].dst);
                    _input[i].layer->Reshape(_input[i].src, _input[i].buf, _input[i].dst);
                }
            }

            for (size_t i = 0; i < _stages.size(); ++i)
            {
                _stages[i].layer->Setup(_stages[i].src, _stages[i].buf, _stages[i].dst);
                _stages[i].layer->Reshape(_stages[i].src, _stages[i].buf, _stages[i].dst);
            }

            if (dstNames.size())
            {
                _dst.clear();
                for (size_t i = 0; i < dstNames.size(); ++i)
                {
                    bool found = false;
                    for (size_t j = 0; j < _stages.size(); ++j)
                    {
                        const LayerParam & param = _stages[j].layer->Param();
                        if (param.name() == dstNames[i])
                        {
                            _dst.push_back(_stages[j].dst[0]);
                            found = true;
                            break;
                        }
                    }
                    if (!found)
                        return false;
                }
            }

            return true;
        }

        bool GetMetaConst(const String & name, Tensor & value) const
        {
            for (size_t i = 0; i < _param().layers().size(); ++i)
            {
                const LayerParam & layer = _param().layers()[i];
                if (layer.name() == name && layer.type() == LayerTypeMeta && layer.meta().type() == MetaTypeConst)
                {
                    value.Import(layer.meta().alpha());
                    return true;
                }
            }
            return false;
        }

        void Forward()
        {
            SYNET_PERF_FUNC();
            bool ftz = GetFlushToZero();
            SetFlushToZero(true);
            for (size_t i = 0; i < _stages.size(); ++i)
                _stages[i].layer->Forward(_stages[i].src, _stages[i].buf, _stages[i].dst);
            SetFlushToZero(ftz);
        }

#ifdef SYNET_DEBUG_PRINT_ENABLE
        void DebugPrint(std::ostream & os, bool weight)
        {
            for (size_t i = 0; i < _stages.size(); ++i)
            {
                _stages[i].layer->Forward(_stages[i].src, _stages[i].buf, _stages[i].dst);
                os << "Layer: " << _stages[i].layer->Param().name() << " : ";
                os << ValueToString(_stages[i].layer->Param().type()) << " ( ";
                for(size_t j = 0; j < _stages[i].layer->Param().src().size(); ++j)
                    os << _stages[i].layer->Param().src()[j] << " ";
                os << ")." << std::endl;
                if (weight)
                {
                    for (size_t j = 0; j < _stages[i].layer->Weight().size(); ++j)
                        _stages[i].layer->Weight()[j].DebugPrint(os, String("weight[") + ValueToString(j) + "]");
                }
                for (size_t j = 0; j < _stages[i].dst.size(); ++j)
                    _stages[i].dst[j]->DebugPrint(os, String("dst[") + ValueToString(j) + "]");
            }
        }
#endif

        Regions GetRegions(size_t imageW, size_t imageH, Type threshold, Type overlap) const
        {
            size_t netW = _src[0]->Axis(-1);
            size_t netH = _src[0]->Axis(-2);
            Regions regions;
            for (size_t i = 0; i < _dst.size(); ++i)
            {
                TensorPtrs dst(1, _dst[i]);
                const Layer * layer = _back[i];
                Regions candidats;
                if (layer->Param().type() == Synet::LayerTypeYolo)
                    ((YoloLayer<float>*)layer)->GetRegions(dst, netW, netH, threshold, candidats);
                if (layer->Param().type() == Synet::LayerTypeRegion)
                    ((RegionLayer<float>*)layer)->GetRegions(dst, threshold, candidats);
                if (layer->Param().type() == Synet::LayerTypeDetectionOutput)
                    ((DetectionOutputLayer<float>*)layer)->GetRegions(dst, threshold, candidats);
                for (size_t j = 0; j < candidats.size(); ++j)
                {
                    Region & c = candidats[j];
                    c.x *= imageW;
                    c.w *= imageW;
                    c.y *= imageH;
                    c.h *= imageH;
                    bool insert = true;
                    for (size_t k = 0; k < regions.size(); ++k)
                    {
                        Region & r = regions[k];
                        if (c.id == r.id && RelativeIntersection(c, r) >= overlap)
                        {
                            if (c.prob > r.prob)
                               r = c;
                            insert = false;
                            break;
                        }
                    }
                    if (insert)
                        regions.push_back(c);
                }
            }
            return regions;
        }

    private:
        typedef std::shared_ptr<Layer> LayerSharedPtr;
        typedef std::vector<LayerSharedPtr> LayerSharedPtrs;

        typedef std::vector<Tensor> Tensors;
        typedef std::shared_ptr<Tensor> TensorSharedPtr;
        typedef std::vector<TensorSharedPtr> TensorSharedPtrs;

        typedef std::map<String, size_t> NameIndexMap;
        typedef std::set<String> NameSet;

        struct Stage
        {
            LayerPtr layer;
            TensorPtrs src;
            TensorPtrs buf;
            TensorPtrs dst;
        };
        typedef std::vector<Stage> Stages;

        bool _empty;
        NetworkParamHolder _param;
        LayerSharedPtrs _layers;
        TensorSharedPtrs _tensors;

        Stages _input, _stages;
        TensorPtrs _src, _dst;
        LayerPtrs _back;

        bool Init()
        {
            _tensors.clear();
            _input.clear();
            _stages.clear();
            _src.clear();
            _dst.clear();
            _back.clear();

            const size_t bufs = 2;
            TensorPtrs buf;
            for (size_t i = 0; i < bufs; ++i)
            {
                TensorSharedPtr tensor(new Tensor());
                _tensors.push_back(tensor);
                buf.push_back(tensor.get());
            }

            NameIndexMap index;
            NameSet available;
            for (size_t i = 0; i < _layers.size(); ++i)
            {
                Stage stage;
                stage.layer = _layers[i].get();
                const LayerParam & param = stage.layer->Param();
                for (size_t j = 0; j < param.src().size(); ++j)
                {
                    const String & name = param.src()[j];
                    if (index.find(name) != index.end())
                    {
                        stage.src.push_back(_tensors[index[name]].get());
                        if (available.find(name) != available.end())
                            available.erase(name);
                    }
                    else
                        assert(0);
                }
                for (size_t j = 0; j < param.dst().size(); ++j)
                {
                    const String & name = param.dst()[j];
                    if (j < param.src().size() && name == param.src()[j])
                    {
                        stage.dst.push_back(_tensors[index[name]].get());
                    }
                    else  if (index.find(name) != index.end())
                    {
                        assert(0);
                    }
                    else
                    {
                        TensorSharedPtr tensor(new Tensor());
                        tensor->SetName(name);
                        index[name] = _tensors.size();
                        _tensors.push_back(tensor);
                        stage.dst.push_back(tensor.get());
                    }
                    available.insert(name);
                    if (param.type() == LayerTypeInput || (param.type() == LayerTypeMeta && param.meta().type() == MetaTypeInput))
                    {
                        _src.push_back(_tensors.back().get());
                    }
                }
                stage.buf = buf;
                if (param.type() == LayerTypeInput || (param.type() == LayerTypeMeta && param.meta().type() == MetaTypeInput))
                    _input.push_back(stage);
                else
                    _stages.push_back(stage);
            }
            for (NameSet::const_iterator it = available.begin(); it != available.end(); ++it)
            {
                if (InsertDst(*it))
                {
                    _dst.push_back(_tensors[index[*it]].get());
                    for (size_t i = 0; i < _layers.size(); ++i)
                    {
                        if (_layers[i]->Param().name() == *it)
                        {
                            _back.push_back(_layers[i].get());
                            break;
                        }
                    }
                }
            }
            if (!Dynamic())
                Reshape();
            _empty = false;
            return true;
        }

        bool InsertDst(const String & name)
        {
            if (_param().dst().empty())
                return true;
            for (size_t i = 0; i < _param().dst().size(); ++i)
            {
                if (_param().dst()[i] == name)
                    return true;
            }
            return false;
        }

        bool Dynamic()
        {
            for (size_t i = 0; i < _param().layers().size(); ++i)
            {
                const LayerParam & layer = _param().layers()[i];
                if (layer.type() == LayerTypeMeta && layer.meta().type() == MetaTypeInput)
                    return true;
                if (layer.type() == LayerTypeInput && layer.input().shape().empty())
                    return true;
            }
            return false;
        }

        static LayerPtr Create(const LayerParam & param)
        {
            switch (param.type())
            {
            case LayerTypeBatchNorm: return new BatchNormLayer<T>(param);
            case LayerTypeBias: return new BiasLayer<T>(param);
            case LayerTypeCast: return new CastLayer<T>(param);
            case LayerTypeConcat: return new ConcatLayer<T>(param);
            case LayerTypeConst: return new ConstLayer<T>(param);
            case LayerTypeConvolution: return new ConvolutionLayer<T>(param);
            case LayerTypeDetectionOutput: return new DetectionOutputLayer<T>(param);
            case LayerTypeDropout: return new StubLayer<T>(param);
            case LayerTypeEltwise: return new EltwiseLayer<T>(param);
            case LayerTypeExpandDims: return new ExpandDimsLayer<T>(param);
            case LayerTypeFill: return new FillLayer<T>(param);
            case LayerTypeFlatten: return new FlattenLayer<T>(param);
            case LayerTypeGather: return new GatherLayer<T>(param);
            case LayerTypeInnerProduct: return new InnerProductLayer<T>(param);
            case LayerTypeInput: return new InputLayer<T>(param);
            case LayerTypeInterp: return new InterpLayer<T>(param);
            case LayerTypeLog: return new LogLayer<T>(param);
            case LayerTypeLrn: return new LrnLayer<T>(param);
            case LayerTypeMeta: return new MetaLayer<T>(param);
            case LayerTypeNormalize: return new NormalizeLayer<T>(param);
            case LayerTypePad: return new PadLayer<T>(param);
            case LayerTypePermute: return new PermuteLayer<T>(param);
            case LayerTypePooling: return new PoolingLayer<T>(param);
            case LayerTypePriorBox: return new PriorBoxLayer<T>(param);
            case LayerTypeRegion: return new RegionLayer<T>(param);
            case LayerTypeRelu: return new ReluLayer<T>(param);
            case LayerTypeReorg: return new ReorgLayer<T>(param);
            case LayerTypeReshape: return new ReshapeLayer<T>(param);
            case LayerTypeRestrictRange: return new RestrictRangeLayer<T>(param);
            case LayerTypeScale: return new ScaleLayer<T>(param);
            case LayerTypeShortcut: return new ShortcutLayer<T>(param);
            case LayerTypeSigmoid: return new SigmoidLayer<T>(param);
            case LayerTypeSlice: return new SliceLayer<T>(param);
            case LayerTypeSoftmax: return new SoftmaxLayer<T>(param);
            case LayerTypeSqueeze: return new SqueezeLayer<T>(param);
            case LayerTypeStub: return new StubLayer<T>(param);
            case LayerTypeSwitch: return new SwitchLayer<T>(param);
            case LayerTypeUnaryOperation: return new UnaryOperationLayer<T>(param);
            case LayerTypeUnpack: return new UnpackLayer<T>(param);
            case LayerTypeUpsample: return new UpsampleLayer<T>(param);
            case LayerTypeYolo: return new YoloLayer<T>(param);
            default:
                return NULL;
            }
        }

        static SYNET_INLINE Type Overlap(Type x1, Type w1, Type x2, Type w2)
        {
            Type l1 = x1 - w1 / 2;
            Type l2 = x2 - w2 / 2;
            Type left = l1 > l2 ? l1 : l2;
            Type r1 = x1 + w1 / 2;
            Type r2 = x2 + w2 / 2;
            Type right = r1 < r2 ? r1 : r2;
            return right - left;
        }

        static SYNET_INLINE Type Intersection(const Region & a, const Region & b)
        {
            Type w = Overlap(a.x, a.w, b.x, b.w);
            Type h = Overlap(a.y, a.h, b.y, b.h);
            return (w < 0 || h < 0) ? 0 : w*h;
        }

        static SYNET_INLINE Type Union(const Region & a, const Region & b)
        {
            Type i = Intersection(a, b);
            return a.w*a.h + b.w*b.h - i;
        }

        static SYNET_INLINE Type RelativeIntersection(const Region & a, const Region & b)
        {
            return Intersection(a, b) / Union(a, b);
        }
        
        friend class TensorflowToSynet;
    };
}
