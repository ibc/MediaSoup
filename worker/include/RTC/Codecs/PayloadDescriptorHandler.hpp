#ifndef MS_RTC_CODECS_PAYLOAD_DESCRIPTOR_HANDLER_HPP
#define MS_RTC_CODECS_PAYLOAD_DESCRIPTOR_HANDLER_HPP

#include "common.hpp"
#include "RTC/SeqManager.hpp"
#include <deque>

namespace RTC
{
	namespace Codecs
	{
		// Codec payload descriptor.
		struct PayloadDescriptor
		{
			virtual ~PayloadDescriptor() = default;
			virtual void Dump() const    = 0;
		};

		class PictureIdList
		{
			static constexpr uint16_t MaxCurrentLayerPictureIdNum{ 1000u };

		public:
			explicit PictureIdList()
			{
			}
			~PictureIdList()
			{
				this->list.clear();
			}

			void Push(uint16_t pictureId, int16_t layer)
			{
				for (const auto& it : this->list)
				{
					// Layers can be changed only with ordered pictureId values.
					// If pictureId is lower than the previous one, then it has rolled over the max value.
					uint16_t diff = pictureId > it.first
					                  ? pictureId - it.first
					                  : pictureId + RTC::SeqManager<uint16_t, 15>::MaxValue - it.first;

					if (diff > MaxCurrentLayerPictureIdNum)
					{
						this->list.pop_front();
					}
					else
					{
						break;
					}
				}
				this->list.push_back({ pictureId, layer });
			}

			int16_t GetLayer(uint16_t pictureId) const
			{
				if (this->list.size() > 1)
				{
					for (auto it = std::next(this->list.begin()); it != this->list.end(); ++it)
					{
						if (RTC::SeqManager<uint16_t, 15>::IsSeqHigherThan(it->first, pictureId))
						{
							return std::prev(it)->second;
						}
					}
				}

				return -1;
			}

		private:
			std::deque<std::pair<uint16_t, int16_t>> list;
		};

		// Encoding context used by PayloadDescriptorHandler to properly rewrite the
		// PayloadDescriptor.
		class EncodingContext
		{
		public:
			struct Params
			{
				uint8_t spatialLayers{ 1u };
				uint8_t temporalLayers{ 1u };
				bool ksvc{ false };
			};

		public:
			explicit EncodingContext(RTC::Codecs::EncodingContext::Params& params) : params(params)
			{
			}
			virtual ~EncodingContext() = default;

		public:
			uint8_t GetSpatialLayers() const
			{
				return this->params.spatialLayers;
			}
			uint8_t GetTemporalLayers() const
			{
				return this->params.temporalLayers;
			}
			bool IsKSvc() const
			{
				return this->params.ksvc;
			}
			int16_t GetTargetSpatialLayer() const
			{
				return this->targetSpatialLayer;
			}
			int16_t GetTargetTemporalLayer() const
			{
				return this->targetTemporalLayer;
			}
			int16_t GetCurrentSpatialLayer() const
			{
				return this->currentSpatialLayer;
			}
			int16_t GetCurrentTemporalLayer() const
			{
				return this->currentTemporalLayer;
			}
			bool GetIgnoreDtx() const
			{
				return this->ignoreDtx;
			}
			void SetTargetSpatialLayer(int16_t spatialLayer)
			{
				this->targetSpatialLayer = spatialLayer;
			}
			void SetTargetTemporalLayer(int16_t temporalLayer)
			{
				this->targetTemporalLayer = temporalLayer;
			}
			void SetCurrentSpatialLayer(int16_t spatialLayer)
			{
				this->currentSpatialLayer = spatialLayer;
			}
			void SetCurrentTemporalLayer(int16_t temporalLayer)
			{
				this->currentTemporalLayer = temporalLayer;
			}
			void SetIgnoreDtx(bool ignoreDtx)
			{
				this->ignoreDtx = ignoreDtx;
			}
			virtual void SyncRequired() = 0;

			void SetCurrentSpatialLayer(int16_t spatialLayer, uint16_t pictureId)
			{
				if (this->currentSpatialLayer == spatialLayer)
				{
					return;
				}

				this->spatialLayerPictureIdList.Push(pictureId, spatialLayer);

				this->currentSpatialLayer = spatialLayer;
			}
			void SetCurrentTemporalLayer(int16_t temporalLayer, uint16_t pictureId)
			{
				if (this->currentTemporalLayer == temporalLayer)
				{
					return;
				}

				this->temporalLayerPictureIdList.Push(pictureId, temporalLayer);

				this->currentTemporalLayer = temporalLayer;
			}
			int16_t GetCurrentSpatialLayer(uint16_t pictureId) const
			{
				int16_t layer = this->spatialLayerPictureIdList.GetLayer(pictureId);
				if (layer > -1)
				{
					return layer;
				}

				return this->currentSpatialLayer;
			}
			int16_t GetCurrentTemporalLayer(uint16_t pictureId) const
			{
				int16_t layer = this->temporalLayerPictureIdList.GetLayer(pictureId);
				if (layer > -1)
				{
					return layer;
				}

				return this->currentTemporalLayer;
			}

		private:
			Params params;
			int16_t targetSpatialLayer{ -1 };
			int16_t targetTemporalLayer{ -1 };
			int16_t currentSpatialLayer{ -1 };
			int16_t currentTemporalLayer{ -1 };
			bool ignoreDtx{ false };

		private:
			PictureIdList spatialLayerPictureIdList;
			PictureIdList temporalLayerPictureIdList;
		};

		class PayloadDescriptorHandler
		{
		public:
			virtual ~PayloadDescriptorHandler() = default;

		public:
			virtual void Dump() const                                                                = 0;
			virtual bool Process(RTC::Codecs::EncodingContext* context, uint8_t* data, bool& marker) = 0;
			virtual void Restore(uint8_t* data)                                                      = 0;
			virtual uint8_t GetSpatialLayer() const                                                  = 0;
			virtual uint8_t GetTemporalLayer() const                                                 = 0;
			virtual bool IsKeyFrame() const                                                          = 0;
		};
	} // namespace Codecs
} // namespace RTC

#endif
