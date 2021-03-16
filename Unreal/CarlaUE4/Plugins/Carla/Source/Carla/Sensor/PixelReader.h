// Copyright (c) 2017 Computer Vision Center (CVC) at the Universitat Autonoma
// de Barcelona (UAB).
//
// This work is licensed under the terms of the MIT license.
// For a copy, see <https://opensource.org/licenses/MIT>.

#pragma once

#include "CoreGlobals.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Runtime/ImageWriteQueue/Public/ImagePixelData.h"

#include <compiler/disable-ue4-macros.h>
#include <carla/Buffer.h>
#include <carla/sensor/SensorRegistry.h>
#include <compiler/enable-ue4-macros.h>


// =============================================================================
// -- FImageResizer -------------------------------------------------------------
// =============================================================================
/// Struct that stores the information to resize an image if needed before send it to client
struct FImageResizer {
  uint32 MinX = 0;
  uint32 MinY = 0;
  uint32 MaxX = 0;
  uint32 MaxY = 0;
  bool ReduceTexture = false;

  FImageResizer() : ReduceTexture(false) {}
  FImageResizer(bool reduce_texture, uint32 minX, uint32 maxX, uint32 minY, uint32 maxY) :
    MinX(minX), MinY(minY), MaxX(maxX), MaxY(maxY), ReduceTexture(reduce_texture) {}
};


// =============================================================================
// -- FPixelReader -------------------------------------------------------------
// =============================================================================
/// Utils for reading pixels from UTextureRenderTarget2D.
///
/// @todo This class only supports PF_R8G8B8A8 format.
class FPixelReader
{
public:

  /// Copy the pixels in @a RenderTarget into @a BitMap.
  ///
  /// @pre To be called from game-thread.
  static bool WritePixelsToArray(
      UTextureRenderTarget2D &RenderTarget,
      TArray<FColor> &BitMap);

  /// Dump the pixels in @a RenderTarget.
  ///
  /// @pre To be called from game-thread.
  static TUniquePtr<TImagePixelData<FColor>> DumpPixels(
      UTextureRenderTarget2D &RenderTarget);

  /// Asynchronously save the pixels in @a RenderTarget to disk.
  ///
  /// @pre To be called from game-thread.
  static TFuture<bool> SavePixelsToDisk(
      UTextureRenderTarget2D &RenderTarget,
      const FString &FilePath);

  /// Asynchronously save the pixels in @a PixelData to disk.
  ///
  /// @pre To be called from game-thread.
  static TFuture<bool> SavePixelsToDisk(
      TUniquePtr<TImagePixelData<FColor>> PixelData,
      const FString &FilePath);

  /// Convenience function to enqueue a render command that sends the pixels
  /// down the @a Sensor's data stream. It expects a sensor derived from
  /// ASceneCaptureSensor or compatible.
  ///
  /// Note that the serializer needs to define a "header_offset" that it's
  /// allocated in front of the buffer.
  ///
  /// @pre To be called from game-thread.
  template <typename TSensor>
  static void SendPixelsInRenderThread(TSensor &Sensor, const FImageResizer& = FImageResizer());

private:

  /// Copy the pixels in @a RenderTarget into @a Buffer.
  ///
  /// @pre To be called from render-thread.
  static void WritePixelsToBuffer(
      UTextureRenderTarget2D &RenderTarget,
      carla::Buffer &Buffer,
      uint32 Offset,
      FRHICommandListImmediate &InRHICmdList,
      const FImageResizer& = FImageResizer());
};

// =============================================================================
// -- FPixelReader::SendPixelsInRenderThread -----------------------------------
// =============================================================================
template <typename TSensor>
void FPixelReader::SendPixelsInRenderThread(TSensor &Sensor, const FImageResizer& ImageResizer)
{
  TRACE_CPUPROFILER_EVENT_SCOPE(FPixelReader::SendPixelsInRenderThread);
  check(Sensor.CaptureRenderTarget != nullptr);

  if (!Sensor.HasActorBegunPlay() || Sensor.IsPendingKill())
  {
    return;
  }

  /// Blocks until the render thread has finished all it's tasks.
  Sensor.EnqueueRenderSceneImmediate();

  // Enqueue a command in the render-thread that will write the image buffer to
  // the data stream. The stream is created in the capture thus executed in the
  // game-thread.
  ENQUEUE_RENDER_COMMAND(FWritePixels_SendPixelsInRenderThread)
  (
    [&Sensor, Stream=Sensor.GetDataStream(Sensor), &ImageResizer](auto &InRHICmdList) mutable
    {
      TRACE_CPUPROFILER_EVENT_SCOPE_STR("FWritePixels_SendPixelsInRenderThread");

      /// @todo Can we make sure the sensor is not going to be destroyed?
      if (!Sensor.IsPendingKill())
      {
        auto Buffer = Stream.PopBufferFromPool();
        WritePixelsToBuffer(
            *Sensor.CaptureRenderTarget,
            Buffer,
            carla::sensor::SensorRegistry::get<TSensor *>::type::header_offset,
            InRHICmdList,
            ImageResizer);
        if(Buffer.data())
        {
          SCOPE_CYCLE_COUNTER(STAT_CarlaSensorStreamSend);
          TRACE_CPUPROFILER_EVENT_SCOPE_STR("Stream Send");
          Stream.Send(Sensor, std::move(Buffer));
        }
      }
    }
  );
  // Blocks until the render thread has finished all it's tasks
  Sensor.WaitForRenderThreadToFinsih();
}
