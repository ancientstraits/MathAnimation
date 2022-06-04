#include "animation/Animation.h"
#include "core.h"

#include "renderer/Renderer.h"
#include "animation/Styles.h"
#include "animation/AnimationBuilders.h"
#include "utils/CMath.h"

namespace MathAnim
{
	// ------- Private variables --------
	static const char* animationObjectTypeNames[] = {
		"None",
		"Text Object",
		"LaTex Object",
		"Length"
	};

	static const char* animationTypeNames[] = {
		"None",
		"Write In Text",
		"Length",
	};

	static int animObjectUidCounter = 0;
	static int animationUidCounter = 0;

	// ----------------------------- Internal Functions -----------------------------
	static AnimObject deserializeAnimObjectV1(RawMemory& memory);

	static AnimationEx deserializeAnimationExV1(RawMemory& memory);

	// Constants
	constexpr uint32 SERIALIZER_VERSION = 1;
	constexpr uint32 MAGIC_NUMBER = 0xDEADBEEF;

	namespace AnimationManagerEx
	{
		// List of animatable objects, sorted by start time
		static std::vector<AnimObject> mObjects;

		// Internal Functions
		static void deserializeAnimationManagerExV1(RawMemory& memory);

		void addAnimObject(const AnimObject& object)
		{
			bool insertedObject = false;
			for (auto iter = mObjects.begin(); iter != mObjects.end(); iter++)
			{
				// Insert it here. The list will always be sorted
				if (object.frameStart > iter->frameStart)
				{
					mObjects.insert(iter, object);
					insertedObject = true;
					break;
				}
			}

			if (!insertedObject)
			{
				// If we didn't insert the object
				// that means it must start after all the
				// current animObject start times.
				mObjects.push_back(object);
			}
		}

		void addAnimationTo(AnimationEx animation, AnimObject& animObject)
		{
			for (auto iter = animObject.animations.begin(); iter != animObject.animations.end(); iter++)
			{
				// Insert it here. The list will always be sorted
				if (animation.frameStart > iter->frameStart)
				{
					animObject.animations.insert(iter, animation);
					return;
				}
			}

			// If we didn't insert the animation
			// that means it must start after all the
			// current animation start times.
			animObject.animations.push_back(animation);
		}

		void addAnimationTo(AnimationEx animation, int animObjectId)
		{
			for (int ai = 0; ai < mObjects.size(); ai++)
			{
				AnimObject& animObject = mObjects[ai];
				if (animObject.id != animObjectId)
				{
					continue;
				}

				for (auto iter = animObject.animations.begin(); iter != animObject.animations.end(); iter++)
				{
					// Insert it here. The list will always be sorted
					if (animation.frameStart > iter->frameStart)
					{
						animObject.animations.insert(iter, animation);
						return;
					}
				}

				// If we didn't insert the animation
				// that means it must start after all the
				// current animation start times.
				animObject.animations.push_back(animation);
				return;
			}
		}

		bool removeAnimObject(int animObjectId)
		{
			// Remove the anim object
			for (int i = 0; i < mObjects.size(); i++)
			{
				if (mObjects[i].id == animObjectId)
				{
					// Free all animations in this object
					for (int j = 0; j < mObjects[i].animations.size(); j++)
					{
						g_logger_assert(mObjects[i].animations[j].objectId == animObjectId, "How did this happen?");
						mObjects[i].animations[j].free();
					}

					mObjects[i].free();
					mObjects.erase(mObjects.begin() + i);
					return true;
				}
			}

			return false;
		}

		bool removeAnimation(int animObjectId, int animationId)
		{
			// Remove the anim object
			for (int i = 0; i < mObjects.size(); i++)
			{
				if (mObjects[i].id == animObjectId)
				{
					// Free all animations in this object
					for (int j = 0; j < mObjects[i].animations.size(); j++)
					{
						if (mObjects[i].animations[j].id == animationId)
						{
							g_logger_assert(mObjects[i].animations[i].objectId == animObjectId, "How did this happen?");
							mObjects[i].animations[j].free();
							mObjects[i].animations.erase(mObjects[i].animations.begin() + j);
							return true;
						}
					}
				}
			}

			return false;
		}

		bool setAnimObjectTime(int animObjectId, int frameStart, int duration)
		{
			// Remove the animation then reinsert it. That way we make sure the list
			// stays sorted
			AnimObject animObjectCopy;
			int objectIndex = -1;
			for (int i = 0; i < mObjects.size(); i++)
			{
				const AnimObject& animObject = mObjects[i];
				if (animObject.id == animObjectId)
				{
					if (animObject.frameStart == frameStart && animObject.duration == duration)
					{
						// If nothing changed, just return that the change was successful
						return true;
					}

					animObjectCopy = animObject;
					objectIndex = i;
					break;
				}
			}

			if (objectIndex != -1)
			{
				mObjects.erase(mObjects.begin() + objectIndex);
				animObjectCopy.frameStart = frameStart;
				animObjectCopy.duration = duration;
				addAnimObject(animObjectCopy);
				return true;
			}

			return false;
		}

		bool setAnimationTime(int animObjectId, int animationId, int frameStart, int duration)
		{
			// Remove the animation then reinsert it. That way we make sure the list
			// stays sorted
			AnimationEx animationCopy;
			int animationIndex = -1;
			int animObjectIndex = -1;
			for (int i = 0; i < mObjects.size(); i++)
			{
				if (mObjects[i].id == animObjectId)
				{
					// TODO: This is nasty. I should probably use a hash map to store
					// animations and anim objects
					for (int j = 0; j < mObjects[i].animations.size(); j++)
					{
						const AnimationEx& animation = mObjects[i].animations[j];
						if (animation.id == animationId)
						{
							if (animation.frameStart == frameStart && animation.duration == duration)
							{
								// If nothing changed, just return that the change was successful
								return true;
							}

							animationCopy = animation;
							animationIndex = j;
							animObjectIndex = i;
							break;
						}
					}
				}

				if (animationIndex != -1)
				{
					break;
				}
			}

			if (animationIndex != -1)
			{
				mObjects[animObjectIndex].animations.erase(mObjects[animObjectIndex].animations.begin() + animationIndex);
				animationCopy.frameStart = frameStart;
				animationCopy.duration = duration;
				addAnimationTo(animationCopy, mObjects[animObjectIndex]);
				return true;
			}

			return false;

		}

		void setAnimObjectTrack(int animObjectId, int track)
		{
			for (int i = 0; i < mObjects.size(); i++)
			{
				if (mObjects[i].id == animObjectId)
				{
					mObjects[i].timelineTrack = track;
					return;
				}
			}
		}

		void render(NVGcontext* vg, int frame)
		{
			for (auto objectIter = mObjects.begin(); objectIter != mObjects.end(); objectIter++)
			{
				objectIter->isAnimating = false;
				for (auto animIter = objectIter->animations.begin(); animIter != objectIter->animations.end(); animIter++)
				{
					float parentFrameStart = animIter->getParent()->frameStart;
					float absoluteFrameStart = animIter->frameStart + parentFrameStart;
					int animDeathTime = absoluteFrameStart + animIter->duration;
					if (absoluteFrameStart <= frame && frame <= animDeathTime)
					{
						float interpolatedT = ((float)frame - absoluteFrameStart) / (float)animIter->duration;
						animIter->render(vg, interpolatedT);

						objectIter->isAnimating = true;
					}
				}

				int objectDeathTime = objectIter->frameStart + objectIter->duration;
				if (!objectIter->isAnimating && objectIter->frameStart <= frame && frame <= objectDeathTime)
				{
					objectIter->render(vg);
				}
			}
		}

		const AnimObject* getObject(int animObjectId)
		{
			return getMutableObject(animObjectId);
		}

		AnimObject* getMutableObject(int animObjectId)
		{
			for (int i = 0; i < mObjects.size(); i++)
			{
				if (mObjects[i].id == animObjectId)
				{
					return &mObjects[i];
				}
			}

			return nullptr;
		}

		const std::vector<AnimObject>& getAnimObjects()
		{
			return mObjects;
		}

		void serialize(const char* savePath)
		{
			// This data should always be present regardless of file version
			// Container data layout
			// magicNumber   -> uint32
			// version       -> uint32
			RawMemory memory;
			memory.init(sizeof(uint32) + sizeof(uint32));
			memory.write<uint32>(&MAGIC_NUMBER);
			memory.write<uint32>(&SERIALIZER_VERSION);

			// Custom data starts here. Subject to change from version to version
			// numAnimations -> uint32
			// animObjects   -> dynamic
			uint32 numAnimations = (uint32)mObjects.size();
			memory.write<uint32>(&numAnimations);

			// Write out each animation followed by 0xDEADBEEF
			for (int i = 0; i < mObjects.size(); i++)
			{
				mObjects[i].serialize(memory);
				memory.write<uint32>(&MAGIC_NUMBER);
			}
			memory.shrinkToFit();

			FILE* fp = fopen(savePath, "wb");
			fwrite(memory.data, memory.size, 1, fp);
			fclose(fp);

			memory.free();
		}

		void deserialize(const char* loadPath)
		{
			FILE* fp = fopen(loadPath, "rb");
			if (!fp)
			{
				g_logger_warning("Could not load '%s', does not exist.", loadPath);
				return;
			}

			fseek(fp, 0, SEEK_END);
			size_t fileSize = ftell(fp);
			fseek(fp, 0, SEEK_SET);

			RawMemory memory;
			memory.init(fileSize);
			fread(memory.data, fileSize, 1, fp);
			fclose(fp);

			// Read magic number and version then dispatch to appropraite
			// deserializer
			// magicNumber   -> uint32
			// version       -> uint32
			uint32 magicNumber;
			memory.read<uint32>(&magicNumber);
			uint32 serializerVersion;
			memory.read<uint32>(&serializerVersion);

			g_logger_assert(magicNumber == MAGIC_NUMBER, "File '%s' had invalid magic number '0x%8x. File must have been corrupted.", loadPath, magicNumber);
			g_logger_assert((serializerVersion != 0 && serializerVersion <= SERIALIZER_VERSION), "File '%s' saved with invalid version '%d'. Looks like corrupted data.", loadPath, serializerVersion);

			if (serializerVersion == 1)
			{
				deserializeAnimationManagerExV1(memory);
			}
			else
			{
				g_logger_error("AnimationManagerEx serialized with unknown version '%d'.", serializerVersion);
			}

			memory.free();
		}

		const char* getAnimObjectName(AnimObjectType type)
		{
			g_logger_assert((int)type < (int)AnimObjectType::Length && (int)type >= 0, "Invalid type '%d'.", (int)type);
			return animationObjectTypeNames[(int)type];
		}

		const char* getAnimationName(AnimTypeEx type)
		{
			g_logger_assert((int)type < (int)AnimTypeEx::Length && (int)type >= 0, "Invalid type '%d'.", (int)type);
			return animationTypeNames[(int)type];
		}

		// Internal Functions
		static void deserializeAnimationManagerExV1(RawMemory& memory)
		{
			// We're in function V1 so this is a version 1 for sure
			constexpr uint32 version = 1;

			// numAnimations -> uint32
			// animObjects   -> dynamic
			uint32 numAnimations;
			memory.read<uint32>(&numAnimations);

			// Write out each animation followed by 0xDEADBEEF
			for (int i = 0; i < numAnimations; i++)
			{
				AnimObject animObject = AnimObject::deserialize(memory, version);
				mObjects.push_back(animObject);
				uint32 magicNumber;
				memory.read<uint32>(&magicNumber);
				g_logger_assert(magicNumber == MAGIC_NUMBER, "Corrupted animation in file data. Bad magic number '0x%8x'", magicNumber);
			}
		}
	}

	void AnimationEx::render(NVGcontext* vg, float t) const
	{
		switch (type)
		{
		case AnimTypeEx::WriteInText:
			getParent()->as.textObject.renderWriteInAnimation(vg, t, getParent());
			break;
		default:
			// TODO: Add magic_enum
			// g_logger_info("Unknown animation: '%s'", magic_enum::enum_name(type).data());
			g_logger_info("Unknown animation: %d", type);
			break;
		}
	}

	const AnimObject* AnimationEx::getParent() const
	{
		const AnimObject* res = AnimationManagerEx::getObject(objectId);
		g_logger_assert(res != nullptr, "Invalid anim object.");
		return res;
	}

	void AnimationEx::free()
	{
		// TODO: Place any animation freeing in here
	}

	void AnimationEx::serialize(RawMemory& memory) const
	{
		// type         -> uint32
		// objectId     -> int32
		// frameStart   -> int32
		// duration     -> int32
		// id           -> int32
		uint32 animType = (uint32)this->type;
		memory.write<uint32>(&animType);
		memory.write<int32>(&objectId);
		memory.write<int32>(&frameStart);
		memory.write<int32>(&duration);
		memory.write<int32>(&id);
	}

	AnimationEx AnimationEx::deserialize(RawMemory& memory, uint32 version)
	{
		// type         -> uint32
		// objectId     -> int32
		// frameStart   -> int32
		// duration     -> int32
		// id           -> int32
		if (version == 1)
		{
			return deserializeAnimationExV1(memory);
		}

		g_logger_error("AnimationEx serialized with unknown version '%d'. Memory potentially corrupted.", version);
		AnimationEx res;
		res.id = -1;
		res.objectId = -1;
		res.type = AnimTypeEx::None;
		res.duration = 0;
		res.frameStart = 0;
		return res;
	}

	AnimationEx AnimationEx::createDefault(AnimTypeEx type, int32 frameStart, int32 duration, int32 animObjectId)
	{
		AnimationEx res;
		res.id = animationUidCounter++;
		res.frameStart = frameStart;
		res.duration = duration;
		res.objectId = animObjectId;
		res.type = type;

		return res;
	}

	void AnimObject::render(NVGcontext* vg) const
	{
		switch (objectType)
		{
		case AnimObjectType::TextObject:
			this->as.textObject.render(vg, this);
			break;
		default:
			// TODO: Add magic_enum
			// g_logger_info("Unknown animation: '%s'", magic_enum::enum_name(objectType).data());
			g_logger_info("Unknown animation: %d", objectType);
			break;
		}
	}

	void AnimObject::free()
	{
		switch (this->objectType)
		{
		case AnimObjectType::TextObject:
			this->as.textObject.free();
			break;
		case AnimObjectType::LaTexObject:
			this->as.laTexObject.free();
			break;
		}
	}

	void AnimObject::serialize(RawMemory& memory) const
	{
		//   AnimObjectType     -> uint32
		//   Position
		//     X                -> f32
		//     Y                -> f32
		//   Id                 -> int32
		//   FrameStart         -> int32
		//   Duration           -> int32
		//   TimelineTrack      -> int32
		//   AnimationTypeSpecificData (This data will change depending on AnimObjectType)
		uint32 animObjectType = (uint32)objectType;
		memory.write<uint32>(&animObjectType);
		memory.write<float>(&position.x);
		memory.write<float>(&position.y);
		memory.write<int32>(&id);
		memory.write<int32>(&frameStart);
		memory.write<int32>(&duration);
		memory.write<int32>(&timelineTrack);

		switch (objectType)
		{
		case AnimObjectType::TextObject:
			this->as.textObject.serialize(memory);
			break;
		case AnimObjectType::LaTexObject:
			this->as.laTexObject.serialize(memory);
			break;
		}

		// NumAnimations  -> uint32
		// Animations     -> dynamic
		uint32 numAnimations = (uint32)this->animations.size();
		memory.write<uint32>(&numAnimations);
		for (int i = 0; i < numAnimations; i++)
		{
			animations[i].serialize(memory);
		}
	}

	AnimObject AnimObject::deserialize(RawMemory& memory, uint32 version)
	{
		if (version == 1)
		{
			return deserializeAnimObjectV1(memory);
		}

		g_logger_error("AnimObject serialized with unknown version '%d'. Potentially corrupted memory.", version);
		AnimObject res;
		res.animations = {};
		res.frameStart = 0;
		res.duration = 0;
		res.id = -1;
		res.objectType = AnimObjectType::None;
		res.position = {};
		res.timelineTrack = -1;
		return res;
	}

	AnimObject AnimObject::createDefault(AnimObjectType type, int32 frameStart, int32 duration)
	{
		AnimObject res;
		res.id = animObjectUidCounter++;
		res.animations = {};
		res.frameStart = frameStart;
		res.duration = duration;
		res.isAnimating = false;
		res.objectType = type;
		res.position = { 0, 0 };

		switch (type)
		{
		case AnimObjectType::TextObject:
			res.as.textObject = TextObject::createDefault();
			break;
		case AnimObjectType::LaTexObject:
			res.as.laTexObject = LaTexObject::createDefault();
			break;
		}

		return res;
	}

	// ----------------------------- Internal Functions -----------------------------
	static AnimObject deserializeAnimObjectV1(RawMemory& memory)
	{
		AnimObject res;

		// AnimObjectType     -> uint32
		// Position
		//   X                -> f32
		//   Y                -> f32
		// Id                 -> int32
		// FrameStart         -> int32
		// Duration           -> int32
		// TimelineTrack      -> int32
		// AnimationTypeDataSize -> uint64
		// AnimationTypeSpecificData (This data will change depending on AnimObjectType)
		uint32 animObjectType;
		memory.read<uint32>(&animObjectType);
		g_logger_assert(animObjectType < (uint32)AnimObjectType::Length, "Invalid AnimObjectType '%d' from memory. Must be corrupted memory.", animObjectType);
		res.objectType = (AnimObjectType)animObjectType;
		memory.read<float>(&res.position.x);
		memory.read<float>(&res.position.y);
		memory.read<int32>(&res.id);
		memory.read<int32>(&res.frameStart);
		memory.read<int32>(&res.duration);
		memory.read<int32>(&res.timelineTrack);
		animObjectUidCounter = glm::max(animObjectUidCounter, res.id + 1);

		// We're in V1 so this is version 1
		constexpr uint32 version = 1;
		switch (res.objectType)
		{
		case AnimObjectType::TextObject:
			res.as.textObject = TextObject::deserialize(memory, version);
			break;
		case AnimObjectType::LaTexObject:
			res.as.laTexObject = LaTexObject::deserialize(memory, version);
			break;
		default:
			g_logger_error("Unknown anim object type: %d. Corrupted memory.", res.objectType);
			break;
		}

		// NumAnimations  -> uint32
		// Animations     -> dynamic
		uint32 numAnimations;
		memory.read<uint32>(&numAnimations);
		for (int i = 0; i < numAnimations; i++)
		{
			AnimationEx animation = AnimationEx::deserialize(memory, SERIALIZER_VERSION);
			animationUidCounter = glm::max(animationUidCounter, animation.id + 1);
			res.animations.push_back(animation);
		}

		return res;
	}

	AnimationEx deserializeAnimationExV1(RawMemory& memory)
	{
		// type         -> uint32
		// objectId     -> int32
		// frameStart   -> int32
		// duration     -> int32
		// id           -> int32
		AnimationEx res;
		uint32 animType;
		memory.read<uint32>(&animType);
		g_logger_assert(animType < (uint32)AnimTypeEx::Length, "Invalid animation type '%d' from memory. Must be corrupted memory.", animType);
		res.type = (AnimTypeEx)animType;
		memory.read<int32>(&res.objectId);
		memory.read<int32>(&res.frameStart);
		memory.read<int32>(&res.duration);
		memory.read<int32>(&res.id);

		return res;
	}

	namespace AnimationManager
	{
		static std::vector<Animation> mAnimations;
		static std::vector<Style> mStyles;

		static std::vector<PopAnimation> mAnimationPops = std::vector<PopAnimation>();

		static std::vector<TranslateAnimation> mTranslationAnimations = std::vector<TranslateAnimation>();

		static std::vector<Interpolation> mInterpolations = std::vector<Interpolation>();

		static float mTime = 0.0f;
		static float mLastAnimEndTime = 0.0f;

		void addAnimation(Animation& animation, const Style& style)
		{
			mLastAnimEndTime += animation.delay;
			animation.startTime = mLastAnimEndTime;
			mLastAnimEndTime += animation.duration;

			mAnimations.push_back(animation);
			mStyles.push_back(style);
		}

		void addInterpolation(Animation& animation)
		{
			//	Interpolation interpolation;
			//	interpolation.ogAnimIndex = mBezier2Animations.size() - 1;
			//	interpolation.ogAnim = mBezier2Animations[interpolation.ogAnimIndex];
			//	interpolation.ogP0Index = mFilledCircleAnimations.size() - 3;
			//	interpolation.ogP1Index = mFilledCircleAnimations.size() - 2;
			//	interpolation.ogP2Index = mFilledCircleAnimations.size() - 1;

			//	mLastAnimEndTime += animation.delay;
			//	animation.startTime = mLastAnimEndTime;
			//	mLastAnimEndTime += animation.duration;

			//	interpolation.newAnim = animation;
			//	mInterpolations.emplace_back(interpolation);
		}

		void popAnimation(AnimType animationType, float delay, float fadeOutTime)
		{
			PopAnimation pop;
			pop.animType = animationType;
			pop.startTime = mLastAnimEndTime + delay;
			pop.fadeOutTime = fadeOutTime;

			pop.index = mAnimations.size() - 1;

			mAnimationPops.emplace_back(pop);
		}

		void translateAnimation(AnimType animationType, const Vec2& translation, float duration, float delay)
		{
			//	TranslateAnimation anim;
			//	anim.animType = animationType;
			//	anim.duration = duration;
			//	anim.translation = translation;

			//	anim.startTime = mLastAnimEndTime + delay;

			//	switch (animationType)
			//	{
			//	case AnimType::Bezier1Animation:
			//		anim.index = mBezier1Animations.size() - 1;
			//		break;
			//	case AnimType::Bezier2Animation:
			//		anim.index = mBezier2Animations.size() - 1;
			//		break;
			//	case AnimType::BitmapAnimation:
			//		anim.index = mBitmapAnimations.size() - 1;
			//		break;
			//	case AnimType::ParametricAnimation:
			//		anim.index = mParametricAnimations.size() - 1;
			//		break;
			//	case AnimType::TextAnimation:
			//		anim.index = mTextAnimations.size() - 1;
			//		break;
			//	case AnimType::FilledBoxAnimation:
			//		anim.index = mFilledBoxAnimations.size() - 1;
			//		break;
			//	case AnimType::FilledCircleAnimation:
			//		anim.index = mFilledCircleAnimations.size() - 1;
			//		break;
			//	}

			//	mTranslationAnimations.emplace_back(anim);
		}

		void drawParametricAnimation(Animation& genericAnimation, const Style& style)
		{
			if (mTime < genericAnimation.startTime) return;
			Style styleToUse = style;

			float lerp = glm::clamp((mTime - genericAnimation.startTime) / genericAnimation.duration, 0.0f, 1.0f);

			ParametricAnimation& anim = genericAnimation.as.parametric;
			float percentToDo = ((anim.endT - anim.startT) * lerp) / (anim.endT - anim.startT);

			float t = anim.startT;
			const float granularity = ((anim.endT - anim.startT) * percentToDo) / anim.granularity;
			for (int i = 0; i < anim.granularity; i++)
			{
				if (i < anim.granularity - 1 && style.lineEnding == CapType::Arrow)
				{
					styleToUse.lineEnding = CapType::Flat;
				}
				else if (style.lineEnding == CapType::Arrow)
				{
					styleToUse.lineEnding = CapType::Arrow;
				}

				Vec2 coord = anim.parametricEquation(t) + anim.translation;
				float nextT = t + granularity;
				Vec2 nextCoord = anim.parametricEquation(nextT) + anim.translation;
				Renderer::drawLine(Vec2{ coord.x, coord.y }, Vec2{ nextCoord.x, nextCoord.y }, styleToUse);
				t = nextT;
			}
		}

		void drawTextAnimation(Animation& genericAnimation, const Style& style)
		{
			if (mTime < genericAnimation.startTime) return;

			TextAnimation& anim = genericAnimation.as.text;
			std::string tmpStr = std::string(anim.text);
			int numCharsToShow = glm::clamp((int)((mTime - genericAnimation.startTime) / anim.typingTime), 0, (int)tmpStr.length());
			Renderer::drawString(tmpStr.substr(0, numCharsToShow), *anim.font, Vec2{ anim.position.x, anim.position.y }, anim.scale, style.color);
		}

		void drawBitmapAnimation(Animation& genericAnimation, const Style& style)
		{
			if (mTime < genericAnimation.startTime) return;
			int numSquaresToShow = glm::clamp((int)(((mTime - genericAnimation.startTime) / genericAnimation.duration) * 16 * 16), 0, 16 * 16);
			BitmapAnimation& anim = genericAnimation.as.bitmap;

			int randNumber = rand() % 100;
			const Vec2 gridSize = Vec2{
				anim.canvasSize.x / 16.0f,
				anim.canvasSize.y / 16.0f
			};
			for (int y = 0; y < 16; y++)
			{
				for (int x = 0; x < 16; x++)
				{
					if (anim.bitmapState[y][x])
					{
						Vec2 position = anim.canvasPosition + Vec2{ (float)x * gridSize.x, (float)y * gridSize.y };
						Style newStyle = style;
						newStyle.color = anim.bitmap[y][x];
						Renderer::drawFilledSquare(position, gridSize, newStyle);
					}

					while (numSquaresToShow > anim.bitmapSquaresShowing && randNumber >= 49)
					{
						int randX = rand() % 16;
						int randY = rand() % 16;
						while (anim.bitmapState[randY][randX])
						{
							randX = rand() % 16;
							randY = rand() % 16;
						}

						anim.bitmapState[randY][randX] = true;
						anim.bitmapSquaresShowing++;
					}
				}
			}
		}

		void drawBezier1Animation(Animation& genericAnimation, const Style& style)
		{
			if (mTime < genericAnimation.startTime) return;
			Style styleToUse = style;

			// 0, 1 
			float lerp = glm::clamp((mTime - genericAnimation.startTime) / genericAnimation.duration, 0.0f, 1.0f);
			float percentToDo = lerp;
			Bezier1Animation& anim = genericAnimation.as.bezier1;

			float t = 0;
			const float granularity = percentToDo / anim.granularity;
			for (int i = 0; i < anim.granularity; i++)
			{
				if (i < anim.granularity - 1 && style.lineEnding == CapType::Arrow)
				{
					styleToUse.lineEnding = CapType::Flat;
				}
				else if (style.lineEnding == CapType::Arrow)
				{
					styleToUse.lineEnding = CapType::Arrow;
				}

				Vec2 coord = CMath::bezier1(anim.p0 + Vec2{}, anim.p1 + Vec2{}, t);
				float nextT = t + granularity;
				Vec2 nextCoord = CMath::bezier1(anim.p0 + Vec2{}, anim.p1 + Vec2{}, nextT);
				Renderer::drawLine(Vec2{ coord.x, coord.y }, Vec2{ nextCoord.x, nextCoord.y }, styleToUse);
				t = nextT;
			}
		}

		void drawBezier2Animation(Animation& genericAnimation, const Style& style)
		{
			if (mTime < genericAnimation.startTime) return;
			Style styleToUse = style;

			float lerp = glm::clamp((mTime - genericAnimation.startTime) / genericAnimation.duration, 0.0f, 1.0f);
			float percentToDo = (genericAnimation.duration * lerp) / genericAnimation.duration;
			Bezier2Animation& anim = genericAnimation.as.bezier2;

			float t = 0;
			const float granularity = percentToDo / anim.granularity;
			for (int i = 0; i < anim.granularity; i++)
			{
				if (i < anim.granularity - 1 && style.lineEnding == CapType::Arrow)
				{
					styleToUse.lineEnding = CapType::Flat;
				}
				else if (style.lineEnding == CapType::Arrow)
				{
					styleToUse.lineEnding = CapType::Arrow;
				}

				Vec2 coord = CMath::bezier2(anim.p0 + Vec2{}, anim.p1 + Vec2{}, anim.p2 + Vec2{}, t);
				float nextT = t + granularity;
				Vec2 nextCoord = CMath::bezier2(anim.p0 + Vec2{}, anim.p1 + Vec2{}, anim.p2 + Vec2{}, nextT);
				Renderer::drawLine(Vec2{ coord.x, coord.y }, Vec2{ nextCoord.x, nextCoord.y }, styleToUse);
				t = nextT;
			}
		}

		static void interpolate(Interpolation& anim)
		{
			//	if (mTime <= anim.newAnim.startTime) return;

			//	float lerp = glm::clamp((mTime - anim.newAnim.startTime) / anim.newAnim.duration, 0.0f, 1.0f);
			//	Bezier2Animation& ogAnim = mBezier2Animations.at(anim.ogAnimIndex);
			//	ogAnim.p0 += (anim.newAnim.p0 - ogAnim.p0) * lerp;
			//	ogAnim.p1 += (anim.newAnim.p1 - ogAnim.p1) * lerp;
			//	ogAnim.p2 += (anim.newAnim.p2 - ogAnim.p2) * lerp;

			//	mFilledCircleAnimations.at(anim.ogP0Index).position = ogAnim.p0;
			//	mFilledCircleAnimations.at(anim.ogP1Index).position = ogAnim.p1;
			//	mFilledCircleAnimations.at(anim.ogP2Index).position = ogAnim.p2;
		}

		void drawFilledCircleAnimation(Animation& genericAnimation, const Style& style)
		{
			if (mTime < genericAnimation.startTime) return;

			float percentToDo = glm::clamp((mTime - genericAnimation.startTime) / genericAnimation.duration, 0.0f, 1.0f);
			FilledCircleAnimation& anim = genericAnimation.as.filledCircle;

			float t = 0;
			float sectorSize = (percentToDo * 360.0f) / (float)anim.numSegments;
			for (int i = 0; i < anim.numSegments; i++)
			{
				float x = anim.radius * glm::cos(glm::radians(t));
				float y = anim.radius * glm::sin(glm::radians(t));
				float nextT = t + sectorSize;
				float nextX = anim.radius * glm::cos(glm::radians(nextT));
				float nextY = anim.radius * glm::sin(glm::radians(nextT));

				Renderer::drawFilledTriangle(anim.position + Vec2{}, anim.position + Vec2{ x, y }, anim.position + Vec2{ nextX, nextY }, style);

				t += sectorSize;
			}
		}

		void drawFilledBoxAnimation(Animation& genericAnimation, const Style& style)
		{
			if (mTime < genericAnimation.startTime) return;

			float percentToDo = glm::clamp((mTime - genericAnimation.startTime) / genericAnimation.duration, 0.0f, 1.0f);
			FilledBoxAnimation& anim = genericAnimation.as.filledBox;

			switch (anim.fillDirection)
			{
			case Direction::Up:
				Renderer::drawFilledSquare(anim.center - (anim.size / 2.0f), Vec2{ anim.size.x, anim.size.y * percentToDo }, style);
				break;
			case Direction::Down:
				Renderer::drawFilledSquare(anim.center + (anim.size / 2.0f) - Vec2{ anim.size.x, anim.size.y * percentToDo },
					Vec2{ anim.size.x, anim.size.y * percentToDo }, style);
				break;
			case Direction::Right:
				Renderer::drawFilledSquare(anim.center - (anim.size / 2.0f), Vec2{ anim.size.x * percentToDo, anim.size.y }, style);
				break;
			case Direction::Left:
				Renderer::drawFilledSquare(anim.center + (anim.size / 2.0f) - Vec2{ anim.size.x * percentToDo, anim.size.y },
					Vec2{ anim.size.x * percentToDo, anim.size.y }, style);
				break;
			}
		}

		static void popAnim(PopAnimation& anim)
		{
			if (anim.startTime - mTime < 0)
			{
				mAnimations.at(anim.index).startTime = FLT_MAX;
			}
			else
			{
				float percent = (anim.startTime - mTime) / anim.fadeOutTime;
				mStyles.at(anim.index).color.a = percent;
			}
		}

		void update(float dt)
		{
			mTime += dt;
			for (int i = 0; i < mAnimations.size(); i++)
			{
				Animation& anim = mAnimations[i];
				const Style& style = mStyles.at(i);
				anim.drawAnimation(anim, style);
			}

			//for (int i = 0; i < mInterpolations.size(); i++)
			//{
			//	Interpolation& interpolation = mInterpolations[i];
			//	interpolate(interpolation);
			//}

			for (int i = 0; i < mAnimationPops.size(); i++)
			{
				PopAnimation& anim = mAnimationPops[i];
				if (anim.index >= 0 && mTime >= anim.startTime - anim.fadeOutTime)
				{
					popAnim(anim);
				}
			}
			//for (int i = 0; i < mTranslationAnimations.size(); i++)
			//{
			//	TranslateAnimation& anim = mTranslationAnimations[i];
			//	if (mTime >= anim.startTime)
			//	{
			//		if (anim.index >= 0)
			//		{
			//			float delta = glm::clamp(dt / anim.duration, 0.0f, 1.0f);
			//			float overallDelta = glm::clamp((mTime - anim.startTime) / anim.duration, 0.0f, 1.0f);
			//			switch (anim.animType)
			//			{
			//			case AnimType::Bezier1Animation:
			//				mBezier1Animations.at(anim.index).p0 += delta * anim.translation;
			//				mBezier1Animations.at(anim.index).p1 += delta * anim.translation;
			//				break;
			//			case AnimType::Bezier2Animation:
			//				mBezier2Animations.at(anim.index).p0 += delta * anim.translation;
			//				mBezier2Animations.at(anim.index).p1 += delta * anim.translation;
			//				mBezier2Animations.at(anim.index).p2 += delta * anim.translation;
			//				break;
			//			case AnimType::BitmapAnimation:
			//				mBitmapAnimations.at(anim.index).canvasPosition += delta * anim.translation;
			//				break;
			//			case AnimType::ParametricAnimation:
			//				mParametricAnimations.at(anim.index).translation = overallDelta * anim.translation;
			//				break;
			//			case AnimType::TextAnimation:
			//				mTextAnimations.at(anim.index).position += delta * anim.translation;
			//				break;
			//			case AnimType::FilledBoxAnimation:
			//				mFilledBoxAnimations.at(anim.index).center += delta * anim.translation;
			//				break;
			//			case AnimType::FilledCircleAnimation:
			//				mFilledCircleAnimations.at(anim.index).position += delta * anim.translation;
			//				break;
			//			}
			//		}
			//	}
			//}
		}

		void reset()
		{
			mTime = 0.0f;
		}
	}
}