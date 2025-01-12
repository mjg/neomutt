/**
 * @file
 * Quoted style
 *
 * @authors
 * Copyright (C) 2021-2023 Richard Russon <rich@flatcap.org>
 *
 * @copyright
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 2 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @page color_qstyle Quoted style
 *
 * Quoted style
 */

#include "config.h"
#include <stdbool.h>
#include "mutt/lib.h"
#include "qstyle.h"
#include "quoted.h"

/**
 * qstyle_free - Free a single QuoteStyle object
 * @param ptr QuoteStyle to free
 *
 * @note Use qstyle_free_tree() to free the entire tree
 */
static void qstyle_free(struct QuoteStyle **ptr)
{
  if (!ptr || !*ptr)
    return;

  struct QuoteStyle *qc = *ptr;
  FREE(&qc->prefix);

  FREE(ptr);
}

/**
 * qstyle_free_tree - Free an entire tree of QuoteStyle
 * @param[out] quote_list Quote list to free
 *
 * @note Use qstyle_free() to free a single object
 */
void qstyle_free_tree(struct QuoteStyle **quote_list)
{
  struct QuoteStyle *next = NULL;

  while (*quote_list)
  {
    if ((*quote_list)->down)
      qstyle_free_tree(&((*quote_list)->down));
    next = (*quote_list)->next;
    qstyle_free(quote_list);
    *quote_list = next;
  }
}

/**
 * qstyle_new - Create a new QuoteStyle
 * @retval ptr New QuoteStyle
 */
static struct QuoteStyle *qstyle_new(void)
{
  return MUTT_MEM_CALLOC(1, struct QuoteStyle);
}

/**
 * qstyle_insert - Insert a new quote colour class into a list
 * @param[in]     quote_list List of quote colours
 * @param[in]     new_class  New quote colour to inset
 * @param[in]     index      Index to insert at
 * @param[in,out] q_level    Quote level
 */
static void qstyle_insert(struct QuoteStyle *quote_list,
                          struct QuoteStyle *new_class, int index, int *q_level)
{
  struct QuoteStyle *q_list = quote_list;
  new_class->quote_n = -1;

  while (q_list)
  {
    if (q_list->quote_n >= index)
    {
      q_list->quote_n++;
      q_list->attr_color = quoted_colors_get(q_list->quote_n);
    }
    if (q_list->down)
    {
      q_list = q_list->down;
    }
    else if (q_list->next)
    {
      q_list = q_list->next;
    }
    else
    {
      while (!q_list->next)
      {
        q_list = q_list->up;
        if (!q_list)
          break;
      }
      if (q_list)
        q_list = q_list->next;
    }
  }

  new_class->quote_n = index;
  new_class->attr_color = quoted_colors_get(index);
  (*q_level)++;
}

/**
 * qstyle_classify - Find a style for a string
 * @param[out] quote_list   List of quote colours
 * @param[in]  qptr         String to classify
 * @param[in]  length       Length of string
 * @param[out] force_redraw Set to true if a screen redraw is needed
 * @param[out] q_level      Quoting level
 * @retval ptr Quoting style
 */
struct QuoteStyle *qstyle_classify(struct QuoteStyle **quote_list, const char *qptr,
                                   size_t length, bool *force_redraw, int *q_level)
{
  struct QuoteStyle *q_list = *quote_list;
  struct QuoteStyle *qc = NULL, *tmp = NULL, *ptr = NULL, *save = NULL;
  const char *tail_qptr = NULL;
  size_t offset, tail_lng;
  int index = -1;

  /* classify quoting prefix */
  while (q_list)
  {
    if (length <= q_list->prefix_len)
    {
      /* case 1: check the top level nodes */

      if (mutt_strn_equal(qptr, q_list->prefix, length))
      {
        if (length == q_list->prefix_len)
          return q_list; /* same prefix: return the current class */

        /* found shorter prefix */
        if (tmp)
        {
          /* found another branch for which tmp is a shorter prefix */

          /* save the next sibling for later */
          save = q_list->next;

          /* unlink q_list from the top level list */
          if (q_list->next)
            q_list->next->prev = q_list->prev;
          if (q_list->prev)
            q_list->prev->next = q_list->next;

          /* at this point, we have a tmp->down; link q_list to it */
          ptr = tmp->down;
          /* sibling order is important here, q_list should be linked last */
          while (ptr->next)
            ptr = ptr->next;
          ptr->next = q_list;
          q_list->next = NULL;
          q_list->prev = ptr;
          q_list->up = tmp;

          index = q_list->quote_n;

          /* next class to test; as above, we shouldn't go down */
          q_list = save;
        }
        else
        {
          /* add a node above q_list */
          tmp = qstyle_new();
          tmp->prefix = mutt_strn_dup(qptr, length);
          tmp->prefix_len = length;

          /* replace q_list by tmp in the top level list */
          if (q_list->next)
          {
            tmp->next = q_list->next;
            q_list->next->prev = tmp;
          }
          if (q_list->prev)
          {
            tmp->prev = q_list->prev;
            q_list->prev->next = tmp;
          }

          /* make q_list a child of tmp */
          tmp->down = q_list;
          q_list->up = tmp;

          /* q_list has no siblings for now */
          q_list->next = NULL;
          q_list->prev = NULL;

          /* update the root if necessary */
          if (q_list == *quote_list)
            *quote_list = tmp;

          index = q_list->quote_n;

          /* tmp should be the return class too */
          qc = tmp;

          /* next class to test; if tmp is a shorter prefix for another
           * node, that node can only be in the top level list, so don't
           * go down after this point */
          q_list = tmp->next;
        }

        /* we found a shorter prefix, so certain quotes have changed classes */
        *force_redraw = true;
        continue;
      }
      else
      {
        /* shorter, but not a substring of the current class: try next */
        q_list = q_list->next;
        continue;
      }
    }
    else
    {
      /* case 2: try subclassing the current top level node */

      /* tmp != NULL means we already found a shorter prefix at case 1 */
      if (!tmp && mutt_strn_equal(qptr, q_list->prefix, q_list->prefix_len))
      {
        /* ok, it's a subclass somewhere on this branch */

        ptr = q_list;
        offset = q_list->prefix_len;

        q_list = q_list->down;
        tail_lng = length - offset;
        tail_qptr = qptr + offset;

        while (q_list)
        {
          if (length <= q_list->prefix_len)
          {
            if (mutt_strn_equal(tail_qptr, (q_list->prefix) + offset, tail_lng))
            {
              /* same prefix: return the current class */
              if (length == q_list->prefix_len)
                return q_list;

              /* found shorter common prefix */
              if (!tmp)
              {
                /* add a node above q_list */
                tmp = qstyle_new();
                tmp->prefix = mutt_strn_dup(qptr, length);
                tmp->prefix_len = length;

                /* replace q_list by tmp */
                if (q_list->next)
                {
                  tmp->next = q_list->next;
                  q_list->next->prev = tmp;
                }
                if (q_list->prev)
                {
                  tmp->prev = q_list->prev;
                  q_list->prev->next = tmp;
                }

                /* make q_list a child of tmp */
                tmp->down = q_list;
                tmp->up = q_list->up;
                q_list->up = tmp;
                if (tmp->up->down == q_list)
                  tmp->up->down = tmp;

                /* q_list has no siblings */
                q_list->next = NULL;
                q_list->prev = NULL;

                index = q_list->quote_n;

                /* tmp should be the return class too */
                qc = tmp;

                /* next class to test */
                q_list = tmp->next;
              }
              else
              {
                /* found another branch for which tmp is a shorter prefix */

                /* save the next sibling for later */
                save = q_list->next;

                /* unlink q_list from the top level list */
                if (q_list->next)
                  q_list->next->prev = q_list->prev;
                if (q_list->prev)
                  q_list->prev->next = q_list->next;

                /* at this point, we have a tmp->down; link q_list to it */
                ptr = tmp->down;
                while (ptr->next)
                  ptr = ptr->next;
                ptr->next = q_list;
                q_list->next = NULL;
                q_list->prev = ptr;
                q_list->up = tmp;

                index = q_list->quote_n;

                /* next class to test */
                q_list = save;
              }

              /* we found a shorter prefix, so we need a redraw */
              *force_redraw = true;
              continue;
            }
            else
            {
              q_list = q_list->next;
              continue;
            }
          }
          else
          {
            /* longer than the current prefix: try subclassing it */
            if (!tmp && mutt_strn_equal(tail_qptr, (q_list->prefix) + offset,
                                        q_list->prefix_len - offset))
            {
              /* still a subclass: go down one level */
              ptr = q_list;
              offset = q_list->prefix_len;

              q_list = q_list->down;
              tail_lng = length - offset;
              tail_qptr = qptr + offset;

              continue;
            }
            else
            {
              /* nope, try the next prefix */
              q_list = q_list->next;
              continue;
            }
          }
        }

        /* still not found so far: add it as a sibling to the current node */
        if (!qc)
        {
          tmp = qstyle_new();
          tmp->prefix = mutt_strn_dup(qptr, length);
          tmp->prefix_len = length;

          if (ptr->down)
          {
            tmp->next = ptr->down;
            ptr->down->prev = tmp;
          }
          ptr->down = tmp;
          tmp->up = ptr;

          tmp->quote_n = (*q_level)++;
          tmp->attr_color = quoted_colors_get(tmp->quote_n);

          return tmp;
        }
        else
        {
          if (index != -1)
            qstyle_insert(*quote_list, tmp, index, q_level);

          return qc;
        }
      }
      else
      {
        /* nope, try the next prefix */
        q_list = q_list->next;
        continue;
      }
    }
  }

  if (!qc)
  {
    /* not found so far: add it as a top level class */
    qc = qstyle_new();
    qc->prefix = mutt_strn_dup(qptr, length);
    qc->prefix_len = length;
    qc->quote_n = (*q_level)++;
    qc->attr_color = quoted_colors_get(qc->quote_n);

    if (*quote_list)
    {
      if ((*quote_list)->next)
      {
        qc->next = (*quote_list)->next;
        qc->next->prev = qc;
      }
      (*quote_list)->next = qc;
      qc->prev = *quote_list;
    }
    else
    {
      *quote_list = qc;
    }
  }

  if (index != -1)
    qstyle_insert(*quote_list, tmp, index, q_level);

  return qc;
}

/**
 * qstyle_recurse - Update the quoting styles after colour changes
 * @param quote_list Styles to update
 * @param num_qlevel Number of quote levels
 * @param cur_qlevel Current quote level
 */
static void qstyle_recurse(struct QuoteStyle *quote_list, int num_qlevel, int *cur_qlevel)
{
  if (!quote_list)
    return;

  if (num_qlevel > 0)
  {
    quote_list->attr_color = quoted_colors_get(*cur_qlevel);
    *cur_qlevel = (*cur_qlevel + 1) % num_qlevel;
  }
  else
  {
    quote_list->attr_color = NULL;
  }

  qstyle_recurse(quote_list->down, num_qlevel, cur_qlevel);
  qstyle_recurse(quote_list->next, num_qlevel, cur_qlevel);
}

/**
 * qstyle_recolor - Recolour quotes after colour changes
 * @param quote_list List of quote colours
 */
void qstyle_recolor(struct QuoteStyle *quote_list)
{
  if (!quote_list)
    return;

  int num = quoted_colors_num_used();
  int cur = 0;

  qstyle_recurse(quote_list, num, &cur);
}