#include "VectorTileParser.h"
#include "Compression/lz4.h"
#include "flatbuffers/flatbuffers.h"
#include "geo_flatbuffers/CompressedData_generated.h"
#include "geo_flatbuffers/Tile_generated.h"
#include "geo_flatbuffers/BuildingOverlay_generated.h"
#include "geo_flatbuffers/RoadOverlay_generated.h"
#include "geo_flatbuffers/Buffer_generated.h"

using namespace geo::flatbuffer;

bool FVectorTileParser::DecompressLZ4(const TArray<uint8>& CompressedData, TArray<uint8>& OutDecompressed)
{
    if (CompressedData.Num() < sizeof(flatbuffers::uoffset_t))
    {
        return false;
    }
    
    flatbuffers::Verifier Verifier(CompressedData.GetData(), CompressedData.Num());
    if (!common::VerifyCompressedDataBuffer(Verifier))
    {
        return false;
    }
    
    auto* Compressed = common::GetCompressedData(CompressedData.GetData());
    if (!Compressed || !Compressed->data())
    {
        return false;
    }
    
    int32 OriginLength = Compressed->origin_length();
    if (OriginLength <= 0)
    {
        return false;
    }
    
    OutDecompressed.SetNum(OriginLength);
    
    int32 DecompressedSize = LZ4_decompress_safe(
        reinterpret_cast<const char*>(Compressed->data()->data()),
        reinterpret_cast<char*>(OutDecompressed.GetData()),
        Compressed->data()->size(),
        OriginLength
    );
    
    if (DecompressedSize != OriginLength)
    {
        OutDecompressed.Empty();
        return false;
    }
    
    return true;
}

bool FVectorTileParser::ParseVectorTile(const TArray<uint8>& TileData, FBuildingTile& OutTile)
{
    TArray<uint8> DecompressedData;
    if (!DecompressLZ4(TileData, DecompressedData))
    {
        return false;
    }
    
    flatbuffers::Verifier Verifier(DecompressedData.GetData(), DecompressedData.Num());
    if (!vector_tile::VerifyTileBuffer(Verifier))
    {
        return false;
    }
    
    auto* Tile = vector_tile::GetTile(DecompressedData.GetData());
    if (!Tile)
    {
        return false;
    }
    
    OutTile.TileBasePoint = FVector(
        Tile->tile_base_point_x(),
        Tile->tile_base_point_y(),
        Tile->tile_base_point_z()
    );
    
    auto* Overlays = Tile->overlays();
    auto* OverlayTypes = Tile->overlays_type();
    
    if (!Overlays || !OverlayTypes)
    {
        return false;
    }
    
    for (uint32 i = 0; i < OverlayTypes->size(); ++i)
    {
        if (OverlayTypes->Get(i) != vector_tile::Overlay_BuildingOverlay)
        {
            continue;
        }
        
        auto* BuildingOverlay = Overlays->GetAs<vector_tile::BuildingOverlay>(i);
        if (!BuildingOverlay)
        {
            continue;
        }
        
        auto* Buildings = BuildingOverlay->buildings();
        if (!Buildings)
        {
            continue;
        }
        
        for (uint32 j = 0; j < Buildings->size(); ++j)
        {
            auto* Building = Buildings->Get(j);
            if (!Building)
            {
                continue;
            }
            
            FBuildingData BuildingData;
            BuildingData.Id = Building->id();
            BuildingData.CenterZ = static_cast<float>(Building->build_center_z());
            BuildingData.Height = static_cast<float>(Building->build_height());
            
            if (BuildingData.CenterZ > 1e6f || BuildingData.Height > 1e6f || BuildingData.Height <= 0)
            {
                continue;
            }
            
            auto* VertexBuffer = Building->vertex_as_GeoFloatBuffer();
            if (!VertexBuffer || !VertexBuffer->data())
            {
                continue;
            }
            
            uint32 VertexCount = VertexBuffer->count();
            uint32 Dimension = VertexBuffer->dimmension();
            
            if (Dimension != 3)
            {
                continue;
            }
            
            const float* SrcData = VertexBuffer->data()->data();
            uint32 AvailableFloats = VertexBuffer->data()->size();
            if (AvailableFloats < VertexCount * Dimension)
            {
                continue;
            }

            BuildingData.BottomPoints.SetNum(VertexCount);

            for (uint32 k = 0; k < VertexCount; ++k)
            {
                BuildingData.BottomPoints[k] = FVector(
                    SrcData[k * 3],
                    SrcData[k * 3 + 1],
                    SrcData[k * 3 + 2]
                );
            }
            
            auto* Triangles = Building->triangles();
            if (Triangles)
            {
                BuildingData.Triangles.SetNum(Triangles->size());
                for (uint32 k = 0; k < Triangles->size(); ++k)
                {
                    BuildingData.Triangles[k] = Triangles->Get(k);
                }
            }
            
            auto* Holes = Building->holes();
            if (Holes)
            {
                BuildingData.Holes.SetNum(Holes->size());
                for (uint32 k = 0; k < Holes->size(); ++k)
                {
                    BuildingData.Holes[k] = Holes->Get(k);
                }
            }
            
            OutTile.Buildings.Add(BuildingData);
        }
    }
    
    return OutTile.Buildings.Num() > 0;
}

bool FVectorTileParser::ParseRoadTile(const TArray<uint8>& TileData, FRoadTile& OutTile)
{
    TArray<uint8> DecompressedData;
    if (!DecompressLZ4(TileData, DecompressedData))
    {
        return false;
    }
    
    flatbuffers::Verifier Verifier(DecompressedData.GetData(), DecompressedData.Num());
    if (!vector_tile::VerifyTileBuffer(Verifier))
    {
        return false;
    }
    
    auto* Tile = vector_tile::GetTile(DecompressedData.GetData());
    if (!Tile)
    {
        return false;
    }
    
    OutTile.TileBasePoint = FVector(
        Tile->tile_base_point_x(),
        Tile->tile_base_point_y(),
        Tile->tile_base_point_z()
    );
    
    auto* Overlays = Tile->overlays();
    auto* OverlayTypes = Tile->overlays_type();
    
    if (!Overlays || !OverlayTypes)
    {
        return false;
    }
    
    for (uint32 i = 0; i < OverlayTypes->size(); ++i)
    {
        if (OverlayTypes->Get(i) != vector_tile::Overlay_RoadOverlay)
        {
            continue;
        }
        
        auto* RoadOverlay = Overlays->GetAs<vector_tile::RoadOverlay>(i);
        if (!RoadOverlay)
        {
            continue;
        }
        
        auto* Roads = RoadOverlay->roads();
        if (!Roads)
        {
            continue;
        }
        
        for (uint32 j = 0; j < Roads->size(); ++j)
        {
            auto* Road = Roads->Get(j);
            if (!Road)
            {
                continue;
            }
            
            FRoadData RoadData;
            RoadData.Id = Road->id();
            RoadData.TypeId = Road->type_id();
            
            auto* FloatBuffer = static_cast<const common::GeoFloatBuffer*>(Road->points());
            if (!FloatBuffer || !FloatBuffer->data())
            {
                continue;
            }
            
            uint32 BufferCount = FloatBuffer->count();
            uint32 Dimension = FloatBuffer->dimmension();
            
            if (Dimension != 3)
            {
                continue;
            }
            
            const float* SrcData = FloatBuffer->data()->data();
            uint32 AvailableFloats = FloatBuffer->data()->size();
            if (AvailableFloats < BufferCount * Dimension)
            {
                continue;
            }

            RoadData.Positions.SetNum(BufferCount);

            for (uint32 k = 0; k < BufferCount; ++k)
            {
                RoadData.Positions[k] = FVector(
                    SrcData[k * 3],
                    SrcData[k * 3 + 1],
                    SrcData[k * 3 + 2]
                );
            }
            
            OutTile.Roads.Add(RoadData);
        }
    }
    
    return OutTile.Roads.Num() > 0;
}