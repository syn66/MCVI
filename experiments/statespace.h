#pragma once
#include <assert.h>

#include <map>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

template <typename T, typename Hash = std::hash<T>>
class IndexMap {
 public:
  IndexMap(const std::vector<T> &v) {
    indexToElement = v;
    size_t i = 0;
    for (const auto &e : v) elementToIndex.insert({e, i++});
  }

  /// @brief Return the index of the given element within the list
  size_t getIndex(const T &element) const { return elementToIndex.at(element); }

  /// @brief Get the element at the given index
  const T &at(size_t index) const { return indexToElement.at(index); }

  size_t size() const { return indexToElement.size(); }

  /// @brief Return the map of elements to indices
  const std::unordered_map<T, size_t, Hash> &map() const {
    return elementToIndex;
  }

  /// @brief Return a vector of pointers to elements in order
  const std::vector<T> &vector() const { return indexToElement; }

 private:
  std::unordered_map<T, size_t, Hash> elementToIndex;
  std::vector<T> indexToElement;
};

class StateSpace {
 public:
  StateSpace(const std::map<std::string, std::vector<int>> &factors)
      : _sf_map(mapStateFactors(factors)),
        _size(calcSize()),
        prodSF(calculateProdSF()) {}

  size_t size() const { return _size; }

  /// @brief Retrieve the state number given a state
  int stateIndex(const std::map<std::string, int> &state) const {
    int s = 0;
    for (const auto &[name, sf] : _sf_map)
      s += sf.getIndex(state.at(name)) * prodSF.at(name);
    return s;
  }

  /// @brief Retrieve the state given a state number
  std::map<std::string, int> at(int sI) const {
    std::map<std::string, int> s;
    for (const auto &[name, sf] : _sf_map)
      s[name] = getStateFactorElem(sI, name);
    return s;
  }

  /**
   * @brief Return the index within the state factor of the element of state
   * number sI
   *
   * For example, with state factors
   * ```
   * "loc_name": ["river", "land"]
   * "height": [0.0, 0.1, 0.2]
   * ```
   * then the state `sI` = 3 corresponds to {"land", 0.0}
   * so getStateFactorIndex(3, "loc_name") returns 1
   * and getStateFactorIndex(3, "height") returns 0
   */
  int getStateFactorIndex(int sI, std::string sf_name) const {
    return (sI / prodSF.at(sf_name)) % _sf_map.at(sf_name).size();
  }

  /// @brief Return the element of the given state factor for state number sI
  int getStateFactorElem(int sI, std::string sf_name) const {
    return _sf_map.at(sf_name).at(getStateFactorIndex(sI, sf_name));
  }

  /**
   * @brief Given state number sI, return the number of the state where the
   * index of the given state factor is set to`new_sf_elem_idx`
   *
   * * For example, with state factors
   * ```
   * "loc_name": ["river", "land"]
   * "height": [0.0, 0.1, 0.2]
   * ```
   * the state `sI` = 3 corresponds to {"land", 0}.
   * To change the state to {"land", 0.2}, we want to update the index of the
   * "height" state factor to 2.
   * So we call `updateStateFactorIndex(3, "height", 2)`
   * and get a returned state index of 5.
   */
  int updateStateFactorIndex(int sI, std::string sf_name,
                             int new_sf_elem_idx) const {
    const int curr_idx = getStateFactorIndex(sI, sf_name);
    const int delta = new_sf_elem_idx - curr_idx;
    return sI + delta * prodSF.at(sf_name);
  }

  /// @brief Given state number sI, return the number of the state where the
  /// element of the given state factor is set to `new_elem`
  int updateStateFactor(int sI, std::string sf_name, int new_elem) const {
    return updateStateFactorIndex(sI, sf_name,
                                  _sf_map.at(sf_name).getIndex(new_elem));
  }

 private:
  std::map<std::string, IndexMap<int>> _sf_map;  // ordered by name
  size_t _size;
  std::map<std::string, int> prodSF;  // cumulative product of sf lengths

  /// @brief create a map of state factor names to index maps
  std::map<std::string, IndexMap<int>> mapStateFactors(
      const std::map<std::string, std::vector<int>> &factors) const {
    std::map<std::string, IndexMap<int>> map;
    for (const auto &[name, vals] : factors)
      map.emplace(name, IndexMap<int>(vals));
    return map;
  }

  /// @brief calculate a cumulative product of state factor lengths
  std::map<std::string, int> calculateProdSF() const {
    std::vector<int> _prodSF_vec = {1};
    for (auto sf = _sf_map.crbegin(); sf != _sf_map.crend(); ++sf)
      _prodSF_vec.insert(_prodSF_vec.begin(),
                         _prodSF_vec[0] * sf->second.size());

    std::map<std::string, int> _prodSF;
    int i = 1;
    for (const auto &[name, sf] : _sf_map) {
      _prodSF[name] = _prodSF_vec[i++];
    }

    return _prodSF;
  }

  size_t calcSize() const {
    size_t p = 1;
    for (const auto &[name, sf] : _sf_map) p *= sf.size();
    return p;
  }
};
