/*****************************************************************************
 * Copyright (C), 2023,  Computing & Graphics Research Institute of OPLUS Mobile Comm Corp., Ltd
 * License: No license is required for Oplus internal usage.
 *          No external usage is allowed.
 *
 * File : accessor-reader.h
 *
 * Version: 2.0
 *
 * Date : Feb 2023
 *
 * Author: Computing & Graphics Research Institute
 *
 * ------------------ Revision History: ---------------------
 *  <version>  <date>  <author>  <desc>
 *
 *******************************************************************************/

/**
 *
 */
#pragma once
#include <ph/rt-utils.h>

#include "gltf.h"

#include <stdexcept>

namespace gltf {

/**
 * A class to make it easier to get types from gltf accessors.
 */
class AccessorReader {
public:
    /**
     * @param type Type of the elements. For example, scalar, vector3, etc.
     * @return The number of components in the type.
     * For example, SCALAR would be 1, VEC2 would be 2
     * VEC3 would be 3, MAT2 would be 4, MAT3 would be 9, etc.
     * Defaults to 1 if not a recognized type.
     */
    static std::size_t getComponentCount(int type) {
        std::size_t result;

        switch (type) {
        case TINYGLTF_TYPE_SCALAR:
            result = 1;
            break;
        case TINYGLTF_TYPE_VEC2:
            result = 2;
            break;
        case TINYGLTF_TYPE_VEC3:
            result = 3;
            break;
        case TINYGLTF_TYPE_VEC4:
            result = 4;
            break;
        case TINYGLTF_TYPE_MAT2:
            result = 4;
            break;
        case TINYGLTF_TYPE_MAT3:
            result = 9;
            break;
        case TINYGLTF_TYPE_MAT4:
            result = 16;
            break;

            // If we don't recognize the type.
        default:
            // Indicate zero components.
            result = 0;

            // Log a warning.
            PH_LOGW("Unrecognized GLTF type %d", type);
            break;
        }

        return result;
    }

    /**
     * @param accessor The accessor for which we want to know the component
     * count of its type.
     * @return The number of components in the type.
     * For example, SCALAR would be 1, VEC2 would be 2
     * VEC3 would be 3, MAT2 would be 4, MAT3 would be 9, etc.
     * Defaults to 1 if not a recognized type.
     */
    static std::size_t getComponentCount(const tinygltf::Accessor & accessor) { return getComponentCount(accessor.type); }

    /**
     * @param componentType Type of the elements' components. For example, int, float, etc.
     * @return Size of type in bytes.
     * TINYGLTF_COMPONENT_TYPE_BYTE is 1,
     * TINYGLTF_COMPONENT_TYPE_SHORT is 2, etc.
     */
    static std::size_t getComponentTypeSize(int componentType) {
        std::size_t result;

        switch (componentType) {
        case TINYGLTF_COMPONENT_TYPE_BYTE:
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
            result = 1;
            break;
        case TINYGLTF_COMPONENT_TYPE_SHORT:
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
            result = 2;
            break;
        case TINYGLTF_COMPONENT_TYPE_INT:
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
            result = 4;
            break;
        case TINYGLTF_COMPONENT_TYPE_FLOAT:
            result = 4;
            break;
        case TINYGLTF_COMPONENT_TYPE_DOUBLE:
            result = 8;
            break;

            // If we don't recognize the type.
        default:
            // Indicate zero bytes.
            result = 0;

            // Log a warning.
            PH_LOGW("Unrecognized GLTF component type %d", componentType);
            break;
        }

        return result;
    }

    /**
     * @param accessor The accessor for which we want to know the size of its
     * component types.
     * @return Size of type in bytes.
     * TINYGLTF_COMPONENT_TYPE_BYTE is 1,
     * TINYGLTF_COMPONENT_TYPE_SHORT is 2, etc.
     */
    static std::size_t getComponentTypeSize(const tinygltf::Accessor & accessor) { return getComponentTypeSize(accessor.componentType); }

    /**
     * @return Total number of components in the given accessor.
     * For example, if there are 2 Vec3s, it will return 2 * 3 = 6.
     */
    static std::size_t calculateTotalComponentCount(const tinygltf::Accessor & accessor) { return calculateTotalComponentCount(accessor.count, accessor.type); }

    /**
     * @param count Number of elements.
     * @param type Type of the elements. For example, scalar, vector3, etc.
     * @return Total number of components in the given accessor.
     * For example, if there are 2 Vec3s, it will return 2 * 3 = 6.
     */
    static std::size_t calculateTotalComponentCount(std::size_t count, int type) {
        // Total number of elements * number of components in each element.
        return count * getComponentCount(type);
    }

    /**
     * @return Total number of bytes in the given accessor.
     * For example, if there are 2 Vec3s of type short, it will return 2 * 3 * 2 = 12.
     */
    static std::size_t calculateTotalByteCount(const tinygltf::Accessor & accessor) {
        return calculateTotalByteCount(accessor.count, accessor.type, accessor.componentType);
    }

    /**
     * @param count Number of elements.
     * @param type Type of the elements. For example, scalar, vector3, etc.
     * @param componentType Type of the elements' components. For example, int, float, etc.
     * @return Total number of bytes in the given accessor.
     * For example, if there are 2 Vec3s of type short, it will return 2 * 3 * 2 = 12.
     */
    static std::size_t calculateTotalByteCount(std::size_t count, int type, int componentType) {
        // Total number of components * byte size of each component.
        return calculateTotalComponentCount(count, type) * getComponentTypeSize(componentType);
    }

    /**
     * @return Total number of bytes in each element of the given accessor.
     * For example, if the accessor is of type Vec3 short, it will return 3 * 2 = 6.
     */
    static std::size_t calculateElementByteCount(const tinygltf::Accessor & accessor) {
        return calculateElementByteCount(accessor.type, accessor.componentType);
    }

    /**
     * @param type Type of the elements. For example, scalar, vector3, etc.
     * @param componentType Type of the elements' components. For example, int, float, etc.
     * @return Total number of bytes in each element of the given accessor.
     * For example, if the accessor is of type Vec3 short, it will return 3 * 2 = 6.
     */
    static std::size_t calculateElementByteCount(int type, int componentType) {
        // Number of components per element * byte size of component type.
        return getComponentCount(type) * getComponentTypeSize(componentType);
    }

    /**
     * @param type Type of the elements. For example, scalar, vector3, etc.
     * @return The number of components in the type.
     * For example, SCALAR would be 1, VEC2 would be 2
     * VEC3 would be 3, MAT2 would be 4, MAT3 would be 9, etc.
     * Defaults to 1 if not a recognized type.
     */
    static std::string toTypeString(int type) {
        std::string result;

        switch (type) {
        case TINYGLTF_TYPE_SCALAR:
            result = "SCALAR";
            break;
        case TINYGLTF_TYPE_VEC2:
            result = "VEC2";
            break;
        case TINYGLTF_TYPE_VEC3:
            result = "VEC3";
            break;
        case TINYGLTF_TYPE_VEC4:
            result = "VEC4";
            break;
        case TINYGLTF_TYPE_MAT2:
            result = "MAT2";
            break;
        case TINYGLTF_TYPE_MAT3:
            result = "MAT3";
            break;
        case TINYGLTF_TYPE_MAT4:
            result = "MAT4";
            break;

            // If we don't recognize the type.
        default:
            // Log a warning.
            PH_LOGW("Unrecognized GLTF type %d", type);
            break;
        }

        return result;
    }

    /**
     * @param model A pointer to the model this
     * will access to properly read the accessors.
     */
    AccessorReader(const tinygltf::Model * model): _model(model) {}

    /**
     *
     */
    virtual ~AccessorReader() = default;

    /**
     * @return The model who's accessors are being read.
     */
    const tinygltf::Model * getModel() const { return _model; }

    /**
     * Reads the contents of the given accessor,
     * casts them to type T if it doesn't match the accessor's type,
     * and then appends them to the end of the given vector,
     * resizing it if necessary.
     * @param <ResultType> Type of the vector to be saved to.
     * @param accessor The accessor being read from.
     * @param result The vector results will be added to.
     */
    template<typename ResultType>
    void readAccessor(const tinygltf::Accessor & accessor, std::vector<ResultType> & result) const {
        // Record how big the result is already.
        std::size_t oldResultSize = result.size();

        // Make the vector big enough to hold both its original contents as well as what we are adding to it.
        result.resize(oldResultSize + calculateTotalComponentCount(accessor));

        // Get a pointer to the part of the array we will be saving our results to,
        // which is past whatever was already in the collection.
        ResultType * resultStart = result.data() + oldResultSize;

        // Read from the buffer view, passing the types defined by the accessor.
        readAccessor(accessor, resultStart);
    }

    /**
     * Reads the contents of the given accessor,
     * casts them to type T if it doesn't match the accessor's type,
     * and then appends them to the end of the given vector,
     * resizing it if necessary.
     * @param <ResultType> Type of the vector to be saved to.
     * @param accessorId Id of accessor being read from,
     * which will be selected from the model.
     * @param result The vector results will be added to.
     */
    template<typename ResultType>
    void readAccessor(std::size_t accessorId, std::vector<ResultType> & result) const {
        // Fetch the accessor with the given id.
        const tinygltf::Accessor & accessor = _model->accessors[accessorId];

        // Read from it.
        readAccessor(accessor, result);
    }

    /**
     * Reads the contents of the given accessor,
     * casts them to type T if it doesn't match the accessor's type,
     * and then assigns them to the given array.
     * @param <ResultType> Type of the array to be saved to.
     * @param accessor The accessor being read from.
     * @param result The array results will be added to.
     * The array is expected to already have a number of elements
     * equal to calculateTotalComponentCount(accessor).
     */
    template<typename ResultType>
    void readAccessor(const tinygltf::Accessor & accessor, ResultType * result) const {
        // If there is no buffer.
        if (accessor.bufferView == -1) {
            // Fill result with default value of element.
            std::fill(result, result + accessor.count, ResultType());

            // If there is a buffer.
        } else {
            // Defines which part of the buffer is being used.
            const tinygltf::BufferView & bufferView = _model->bufferViews[accessor.bufferView];

            // Read from the buffer view, passing the types defined by the accessor.
            readBufferView(bufferView, accessor.byteOffset, accessor.count, accessor.type, accessor.componentType, result);
        }

        // Modify the result with its sparse data (if any).
        readSparseData(accessor, result);
    }

    /**
     * Reads the contents of the given accessor,
     * casts them to type T if it doesn't match the accessor's type,
     * and then assigns them to the given array.
     * @param <ResultType> Type of the array to be saved to.
     * @param accessorId Id of accessor being read from,
     * which will be selected from the model.
     * @param result The array results will be added to.
     * The array is expected to already have a number of elements
     * equal to calculateTotalComponentCount(accessor).
     */
    template<typename ResultType>
    void readAccessor(std::size_t accessorId, ResultType * result) const {
        // Fetch the accessor with the given id.
        const tinygltf::Accessor & accessor = _model->accessors[accessorId];

        // Read from it.
        readAccessor(accessor, result);
    }

    /**
     * Reads the contents of the given accessor,
     * casts the componentType to type T if it doesn't match the
     * accessor's componentType,
     * and then appends them to the end of the given vector,
     * resizing it if necessary.
     *
     * Throws an exception if accessor type is not compatible with eigen type.
     * @param <ComponentType> Component type of the result.
     * @param accessor The accessor being read from.
     * @param result The vector results will be added to.
     */
    template<typename ComponentType>
    void readAccessor(const tinygltf::Accessor & accessor, std::vector<Eigen::Matrix<ComponentType, 2, 1>> & result) {
        readAccessorAsDenseEigenType<Eigen::Matrix<ComponentType, 2, 1>, ComponentType>(accessor, TINYGLTF_TYPE_VEC2, result);
    }

    /**
     * Reads the contents of the given accessor,
     * casts the componentType to type T if it doesn't match the
     * accessor's componentType,
     * and then appends them to the end of the given vector,
     * resizing it if necessary.
     *
     * Throws an exception if accessor type is not compatible with eigen type.
     * @param <ComponentType> Component type of the result.
     * @param accessor The accessor being read from.
     * @param result The vector results will be added to.
     */
    template<typename ComponentType>
    void readAccessor(const tinygltf::Accessor & accessor, std::vector<Eigen::Matrix<ComponentType, 3, 1>> & result) {
        readAccessorAsDenseEigenType<Eigen::Matrix<ComponentType, 3, 1>, ComponentType>(accessor, TINYGLTF_TYPE_VEC3, result);
    }

    /**
     * Reads the contents of the given accessor,
     * casts the componentType to type T if it doesn't match the
     * accessor's componentType,
     * and then appends them to the end of the given vector,
     * resizing it if necessary.
     *
     * Throws an exception if accessor type is not compatible with eigen type.
     * @param <ComponentType> Component type of the result.
     * @param accessor The accessor being read from.
     * @param result The vector results will be added to.
     */
    template<typename ComponentType>
    void readAccessor(const tinygltf::Accessor & accessor, std::vector<Eigen::Matrix<ComponentType, 4, 1>> & result) {
        readAccessorAsDenseEigenType<Eigen::Matrix<ComponentType, 4, 1>, ComponentType>(accessor, TINYGLTF_TYPE_VEC4, result);
    }

    /**
     * Reads the contents of the given accessor,
     * casts the componentType to type T if it doesn't match the
     * accessor's componentType,
     * and then appends them to the end of the given vector,
     * resizing it if necessary.
     *
     * Throws an exception if accessor type is not compatible with eigen type.
     * @param <ComponentType> Component type of the result.
     * @param accessor The accessor being read from.
     * @param result The vector results will be added to.
     */
    template<typename ComponentType>
    void readAccessor(const tinygltf::Accessor & accessor, std::vector<Eigen::Matrix<ComponentType, 2, 2>> & result) {
        readAccessorAsDenseEigenType<Eigen::Matrix<ComponentType, 2, 2>, ComponentType>(accessor, TINYGLTF_TYPE_MAT2, result);
    }

    /**
     * Reads the contents of the given accessor,
     * casts the componentType to type T if it doesn't match the
     * accessor's componentType,
     * and then appends them to the end of the given vector,
     * resizing it if necessary.
     *
     * Throws an exception if accessor type is not compatible with eigen type.
     * @param <ComponentType> Component type of the result.
     * @param accessor The accessor being read from.
     * @param result The vector results will be added to.
     */
    template<typename ComponentType>
    void readAccessor(const tinygltf::Accessor & accessor, std::vector<Eigen::Matrix<ComponentType, 3, 3>> & result) {
        readAccessorAsDenseEigenType<Eigen::Matrix<ComponentType, 3, 3>, ComponentType>(accessor, TINYGLTF_TYPE_MAT3, result);
    }

    /**
     * Reads the contents of the given accessor,
     * casts the componentType to type T if it doesn't match the
     * accessor's componentType,
     * and then appends them to the end of the given vector,
     * resizing it if necessary.
     *
     * Throws an exception if accessor type is not compatible with eigen type.
     * @param <ComponentType> Component type of the result.
     * @param accessor The accessor being read from.
     * @param result The vector results will be added to.
     */
    template<typename ComponentType>
    void readAccessor(const tinygltf::Accessor & accessor, std::vector<Eigen::Matrix<ComponentType, 4, 4>> & result) {
        readAccessorAsDenseEigenType<Eigen::Matrix<ComponentType, 4, 4>, ComponentType>(accessor, TINYGLTF_TYPE_MAT4, result);
    }

    /**
     * Reads the contents of the given accessor,
     * casts the componentType to type T if it doesn't match the
     * accessor's componentType,
     * and then appends them to the end of the given vector,
     * resizing it if necessary.
     *
     * Throws an exception if accessor type is not compatible with eigen type.
     * @param <ComponentType> Component type of the result.
     * @param <Rows> Number of rows in the Eigen type.
     * @param <Columns> Number of columns in the Eigen type.
     * @param accessorId Id of accessor being read from,
     * which will be selected from the model.
     * @param result The vector results will be added to.
     */
    template<typename ComponentType, int Rows, int Columns>
    void readAccessor(std::size_t accessorId, std::vector<Eigen::Matrix<ComponentType, Rows, Columns>> & result) {
        // Fetch the accessor with the given id.
        const tinygltf::Accessor & accessor = _model->accessors[accessorId];

        // Read from it.
        readAccessor<ComponentType>(accessor, result);
    }

private:
    /**
     * The model being read from.
     */
    const tinygltf::Model * _model;

    /**
     * Reads the contents of the given accessor,
     * casts the componentType to type T if it doesn't match the
     * accessor's componentType,
     * and then appends them to the end of the given vector,
     * resizing it if necessary.
     *
     * This method is limited to supporting dense non-dynamic eigen types,
     * since their memory is contiguous.
     *
     * Throws an exception if accessor type is not compatible with eigen type.
     * @param <EigenType> Type of the eigen type being read.
     * @param <ComponentType> Component type of the result.
     * @param accessor The accessor being read from.
     * @param result The vector results will be added to.
     */
    template<typename EigenType, typename ComponentType>
    void readAccessorAsDenseEigenType(const tinygltf::Accessor & accessor, int eigenType, std::vector<EigenType> & result) {
        // If the accessor's type does not match what we
        // are attempting to read it as, then fail.
        if (accessor.type != eigenType) {
            PH_THROW("Tried to read an accessor with a type of %d (%s) "
                     "as one with type %d (%s).",
                     accessor.type, toTypeString(accessor.type).c_str(), eigenType, toTypeString(eigenType).c_str());
        }

        // Record how big the result is already.
        std::size_t oldResultSize = result.size();

        // Make the vector big enough to hold both its original contents
        // as well as what we are adding to it.
        result.resize(oldResultSize + accessor.count);

        // Get a pointer to the part of the array we will be saving our
        // results to, which is past whatever was already in the collection.
        // Cast it to the component type.
        ComponentType * resultStart = (ComponentType *) result.data() + oldResultSize;

        // Read from the buffer view, passing the types defined by the accessor.
        readAccessor(accessor, resultStart);
    }

    /**
     * Reads the accessor's sparse data into result.
     * @param accessor The accessor being read from.
     * @param result The collection results will be added to.
     */
    void readSparseIndices(const tinygltf::Accessor & accessor, std::vector<std::size_t> & result) const {
        // Record how big the result is already.
        std::size_t oldResultSize = result.size();

        // Make the vector big enough to hold both its original contents as well as what we are adding to it.
        result.resize(oldResultSize + accessor.sparse.count);

        // Get a pointer to the part of the array we will be saving our results to,
        // which is past whatever was already in the collection.
        std::size_t * resultStart = result.data() + oldResultSize;

        // Defines which part of the buffer is being used.
        const tinygltf::BufferView & bufferView = _model->bufferViews[accessor.sparse.indices.bufferView];

        // Read the buffer into the result array.
        readBufferView(bufferView, accessor.sparse.indices.byteOffset, accessor.sparse.count, TINYGLTF_TYPE_SCALAR, accessor.sparse.indices.componentType,
                       resultStart);
    }

    /**
     * Reads the contents of the given accessor which are assumed
     * to be type AccessorType,
     * casts them to type ResultType if it doesn't match the accessor's type,
     * and then uses push_back to add them to the end of the given vector.
     * <AccessorType> Type of the accessor being read from.
     * <ResultType> Type of the vector to be saved to.
     * @param bufferView The buffer view being read from.
     * @param count Number of elements.
     * @param type Type of the elements. For example, scalar, vector3, etc.
     * @param componentType Type of the elements' components. For example, int, float, etc.
     * @param result The array results will be added to.
     * A number of bytes equal to
     * calculateTotalByteCount(count, type, componentType) will be copied to result.
     */
    template<typename ResultType>
    void readBufferView(const tinygltf::BufferView & bufferView, std::size_t byteOffset, std::size_t count, int type, int componentType,
                        ResultType * result) const {
        // Get the type of the accessor so we know what to cast to what.
        switch (componentType) {
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
            readBufferViewAsType<uint8_t, ResultType>(bufferView, byteOffset, count, type, componentType, result);
            break;
        case TINYGLTF_COMPONENT_TYPE_BYTE:
            readBufferViewAsType<int8_t, ResultType>(bufferView, byteOffset, count, type, componentType, result);
            break;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
            readBufferViewAsType<uint16_t, ResultType>(bufferView, byteOffset, count, type, componentType, result);
            break;
        case TINYGLTF_COMPONENT_TYPE_SHORT:
            readBufferViewAsType<int16_t, ResultType>(bufferView, byteOffset, count, type, componentType, result);
            break;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
            readBufferViewAsType<uint32_t, ResultType>(bufferView, byteOffset, count, type, componentType, result);
            break;
        case TINYGLTF_COMPONENT_TYPE_INT:
            readBufferViewAsType<int32_t, ResultType>(bufferView, byteOffset, count, type, componentType, result);
            break;
        case TINYGLTF_COMPONENT_TYPE_FLOAT:
            readBufferViewAsType<float, ResultType>(bufferView, byteOffset, count, type, componentType, result);
            break;
        case TINYGLTF_COMPONENT_TYPE_DOUBLE:
            readBufferViewAsType<double, ResultType>(bufferView, byteOffset, count, type, componentType, result);
            break;

            // If the component type is not recognized.
        default:
            // Generate error message and throw exception.
            std::ostringstream stringStream;
            stringStream << "Unsupported accessor componentType ";
            stringStream << componentType;
            PH_THROW(stringStream.str().c_str());
            break;
        }
    }

    /**
     * Reads the contents of the given accessor which are assumed
     * to be type AccessorType,
     * casts them to type ResultType if it doesn't match the accessor's type,
     * and then appends them to the end of the given vector,
     * resizing it if necessary.
     * <AccessorType> Type of the accessor being read from.
     * <ResultType> Type of the vector to be saved to.
     * @param bufferView The buffer view being read from.
     * @param count Number of elements.
     * @param type Type of the elements. For example, scalar, vector3, etc.
     * @param componentType Type of the elements' components. For example, int, float, etc.
     * @param result The vector results will be added to.
     */
    template<typename ResultType>
    void readBufferView(const tinygltf::BufferView & bufferView, std::size_t byteOffset, std::size_t count, int type, int componentType,
                        std::vector<ResultType> & result) const {
        // Record how big the result is already.
        std::size_t oldResultSize = result.size();

        // Make the vector big enough to hold both its original contents as well as what we are adding to it.
        result.resize(oldResultSize + calculateTotalComponentCount(count, type));

        // Get a pointer to the part of the array we will be saving our results to,
        // which is past whatever was already in the collection.
        ResultType * resultStart = result.data() + oldResultSize;

        // Read into the vector.
        readBufferView(bufferView, byteOffset, count, type, componentType, resultStart);
    }

    /**
     * Reads the contents of the given accessor which are assumed
     * to be type AccessorType,
     * casts them to type ResultType if it doesn't match the accessor's type,
     * and then uses push_back to add them to the end of the given vector.
     * <AccessorType> Type of the accessor being read from.
     * <ResultType> Type of the vector to be saved to.
     * @param bufferView The accessor being read from.
     * @param result The array results will be added to.
     */
    template<typename BufferViewType, typename ResultType>
    void readBufferViewAsType(const tinygltf::BufferView & bufferView, std::size_t byteOffset, std::size_t count, int type, int componentType,
                              ResultType * result) const {
        // If we don't need to cast the types.
        if (std::is_same<BufferViewType, ResultType>::value) {
            // Then just use the implementation of this method that copies
            // the results over directly via std::memcpy.
            readBufferViewRaw(bufferView, byteOffset, count, type, componentType, result);

            // If we do need to cast the types.
        } else {
            // Holds the data we want to retrieve.
            const tinygltf::Buffer & buffer = _model->buffers[bufferView.buffer];

            // A pointer pointing directly to the start of the buffer's data.
            const uint8_t * bufferStart = (uint8_t *) (buffer.data.data() + byteOffset + bufferView.byteOffset);

            // Calculate the full size of each element in bytes.
            std::size_t elementByteCount = calculateElementByteCount(type, componentType);

            // Determine how many components there are in each element.
            std::size_t componentCount = getComponentCount(type);

            // Determine how much space is between the start of each element.
            std::size_t byteStride;

            // If it's zero, then that means there is no gap between each element, so just move by element size.
            if (bufferView.byteStride == 0) {
                byteStride = elementByteCount;

                // Otherwise, use whatever is saved to byte stride.
            } else {
                byteStride = bufferView.byteStride;
            }

            // Iterate all elements.
            for (std::size_t elementIndex = 0; elementIndex < count; ++elementIndex) {
                // Get a pointer to the part of the result array that this element will be copied to.
                ResultType * resultElementPosition = result + elementIndex * componentCount;

                // Get a pointer to the start of the current element being read from.
                const uint8_t * bufferElementPosition = bufferStart + elementIndex * byteStride;

                // Iterate each component of the element to be copied.
                for (std::size_t componentIndex = 0; componentIndex < componentCount; ++componentIndex) {
                    // Get a pointer to the component being copied from.
                    const uint8_t * bufferComponentPosition = bufferElementPosition + sizeof(BufferViewType) * componentIndex;

                    // Type pun the original data into its specific component type.
                    BufferViewType bufferComponent;
                    std::memcpy(&bufferComponent, bufferComponentPosition, sizeof(BufferViewType));

                    // Cast the buffer value from the component type to the result type
                    // then save it to its position in the result array.
                    resultElementPosition[componentIndex] = (ResultType) bufferComponent;
                }
            }
        }
    }

    /**
     * Reads the contents of the given buffer view,
     * using std::memcpy to transfer them to result
     * without casting.
     * @param bufferView The buffer view being read from.
     * @param byteOffset The amount of offset to add to the preexisting buffer view's offset.
     * @param count Number of elements.
     * @param type Type of the elements. For example, scalar, vector3, etc.
     * @param componentType Type of the elements' components. For example, int, float, etc.
     * @param result The array results will be added to.
     * A number of bytes equal to
     * calculateTotalByteCount(count, type, componentType) will be copied to result.
     */
    void readBufferViewRaw(const tinygltf::BufferView & bufferView, std::size_t byteOffset, std::size_t count, int type, int componentType,
                           void * result) const {
        // Holds the data we want to retrieve.
        const tinygltf::Buffer & buffer = _model->buffers[bufferView.buffer];

        // A pointer pointing directly to the start of the accessor's data.
        uint8_t * bufferStart = (uint8_t *) (buffer.data.data() + byteOffset + bufferView.byteOffset);

        // Calculate the full size of each element in bytes.
        std::size_t elementByteCount = calculateElementByteCount(type, componentType);

        // If there is no gap between elements.
        if (bufferView.byteStride == 0 || bufferView.byteStride == elementByteCount) {
            // Calculate the full size of the accessor in bytes.
            std::size_t byteLength = calculateTotalByteCount(count, type, componentType);

            // Just copy over everything directly.
            std::memcpy(result, bufferStart, byteLength);

            // If there is a gap between elements.
        } else {
            // Cast result array to byte type so we can finely index it.
            uint8_t * resultStart = (uint8_t *) result;

            // Iterate all elements.
            for (std::size_t elementIndex = 0; elementIndex < count; ++elementIndex) {
                // Get a pointer to the part of the result array that this element will be copied to.
                uint8_t * resultPosition = resultStart + elementIndex * elementByteCount;

                // Get a pointer to the part of the buffer being copied from.
                uint8_t * bufferPosition = bufferStart + elementIndex * bufferView.byteStride;

                // Copy the element.
                std::memcpy(resultPosition, bufferPosition, elementByteCount);
            }
        }
    }

    /**
     * Modifies the result array with the sparse data of the given accessor.
     * @param accessor The accessor being read from.
     * @param result The array that will be modified by the accessor's
     * sparse data.
     */
    template<typename ResultType>
    void readSparseData(const tinygltf::Accessor & accessor, ResultType * result) const {
        // Make sure this is a sparse accessor in the first place.
        // If it isn't one, then there is nothing to do.
        if (accessor.sparse.isSparse) {
            // Read the list of indices indicating which elements need to be
            // modified by the sparse accessor.
            std::vector<std::size_t> sparseIndices;
            readSparseIndices(accessor, sparseIndices);

            // Read the list of values to modify the result with.
            std::vector<ResultType>      values;
            const tinygltf::BufferView & valuesBufferView = _model->bufferViews[accessor.sparse.values.bufferView];
            readBufferView(valuesBufferView, accessor.sparse.values.byteOffset, accessor.sparse.count, accessor.type, accessor.componentType, values);

            // Get the pointer to the values array.
            const ResultType * valuesStart = values.data();

            // Get the number of components to copy in each element.
            std::size_t componentCount = getComponentCount(accessor.type);

            // Modify the results according to the indices.
            std::size_t sparseCount = (std::size_t) accessor.sparse.count;
            for (std::size_t index = 0; index < sparseCount; ++index) {
                // Get the index of the element to be modified.
                std::size_t sparseIndex = sparseIndices[index];

                // Get the index in the values array of this element's first component.
                const ResultType * valuesPosition = valuesStart + index * componentCount;

                // Get a pointer to the part of the result array that this element will be copied to.
                ResultType * resultPosition = result + sparseIndex * componentCount;

                // Copy all components of the element from the sparse values to the result.
                std::copy(valuesPosition, valuesPosition + componentCount, resultPosition);
            }
        }
    }
};

} // namespace gltf
